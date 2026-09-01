### timer.c

The component that turns a function into something that runs on its own thread at a fixed rate. Both threads in this program come from here.

```c
#ifndef timer
#	define timer timer
	typedef uint32_t ms;
	typedef struct timer{
		HANDLE(*timer_function)( struct timer* );
		HANDLE threadhandle;
		unsigned long tickrate;
		bool active;
	}*timer;
	typedef typeof(((timer)0)->timer_function) timer_function;
	extern struct{
		HANDLE(*new)( timer_function receiver, ms tickrate );
	}timers;
```

Two things in the typedefs worth pointing out.

`typedef struct timer{ ... }*timer;` typedefs the *pointer*, not the struct. So `timer` as a type name means "a pointer to a timer", and the private functions take `timer this` and get a pointer. The struct tag `struct timer` still names the struct itself, which is what the public callback signature uses.

`typedef typeof(((timer)0)->timer_function) timer_function;` derives the callback type from the member rather than writing the signature out a second time. `(timer)0` is a null pointer that is never dereferenced- `typeof` only looks at the type, so nothing happens at runtime. It is the same instinct as `typeof(box) box` at the bottom of `box.c`: state a type once, and refer back to it everywhere else. Change the callback signature in the struct and everything that uses `timer_function` follows automatically.

`ms` exists so a tickrate cannot be quietly confused with any other number at a call site- `timers.new( tick, (ms){1} )` in `console.c` says what the 1 means.

### One function that is both the starter and the loop

```c
		HANDLE tick( timer this ){
			if( this->active ){
				if( this->threadhandle == 0 ){
					typedef union transparent{
						typeof(&tick) tick;
						LPTHREAD_START_ROUTINE thread;
					}thread_function;
					return this->threadhandle = CreateThread( NULL, 0xffffff, ((thread_function)&tick).thread, this, 0, 0 );
				}
				while( this->active ){
					WaitForSingleObject( this->threadhandle, this->tickrate );
					this->timer_function( this );
				}
				free(this);
			}
			return 0;
		}
```

`tick` is called twice for every timer, and does something different each time.

The first call comes from `new`, on the calling thread, with `threadhandle` still zero. It creates a thread whose start routine is `tick` itself, passing `this` as the thread parameter, stores the handle and returns it.

The second call is that new thread starting up. Now `threadhandle` is set, so it falls through to the loop and stays there.

The transparent union is the same trick as `box.new` uses for `draw`. `CreateThread` demands an `LPTHREAD_START_ROUTINE`, and `tick` is a `HANDLE(*)(struct timer*)`. Rather than casting the function pointer- which is the normal way and throws away all type checking- I declare a union whose members are both signatures and let the transparent attribute accept either. Same effect as a cast at the machine level, but the compiler still knows what the two types are.

`0xffffff` is a 16 megabyte stack, and that is not arbitrary: `console.c`'s tick declares its whole console buffer as a variable length array on the stack every frame. That buffer is one `CHAR_INFO` per cell for a maximized console, so the thread needs room for it.

### The wait is the sleep

```c
					WaitForSingleObject( this->threadhandle, this->tickrate );
```

The thread waits on its own thread handle with a timeout. A thread handle only becomes signalled when the thread exits, and this thread is the one doing the waiting, so it can never be signalled here- the wait always runs out the full timeout. So this is a sleep of `tickrate` milliseconds, written as a wait.

Then `timer_function( this )` is called, and around again. The timer passes itself into the callback, which is the same "c has no thiscall, so pass the object explicitly" compensation as `this()` in `array.c` and `object` in the drawable- the callback gets a handle on its own timer if it wants it.

When `active` goes false the loop ends and the timer frees itself. Nothing in the public interface sets `active` to false though- there is no stop, so in this program the timers run until the process ends. If I wanted to be able to stop one, that is the one member that would need to be reachable, and the natural way would be to put a `stop` in the `timers` vtable rather than exposing the flag.

### Two timers, two threads

`main.c`:

```c
	int main( int argc, char* argv[] ){
		timers.new( drawtick, 2 );
		console.run();
	}
```

and inside `console.run`:

```c
			WaitForSingleObject( timers.new( tick, (ms){1} ), INFINITE );
```

So there are two timers. `drawtick` runs every 2 ms and adds a box; `console.c`'s `tick` runs every 1 ms and paints the whole collection. The main thread creates both and then parks on the render thread's handle forever- and here the wait *does* mean a wait, because the main thread is waiting on a handle that is not its own.

That is the shape the introduction described: one loop producing boxes, another iterating the collection and drawing them, with `timer.c` being the only file that knows a thread was ever involved.
