### console.c

The composition root. Every other component has been written without knowing what it would be used with- `box.c` has never heard of the collection, `drawables.c` has never heard of a box, `timer.c` has never heard of either. This is the file where they meet.

```c
	extern const struct console{
		void(*run)( void );
		void(*add_box)(
			struct grid_position grid_position,
			int16_t height,
			int16_t width,
			struct string boxtext,
			enum text_placement text_placement ,
			enum console_color color
		);
		struct console_size{
			int16_t rows;
			int16_t cols;
		}(*get_size)( void );
	}console;
```

Three functions, and that is the entire surface `main.c` gets.

`struct console_size` is declared inline in the return type of `get_size`. It is still a normal named type afterwards, so `main.c` can hold one, but declaring it where it is used keeps it next to the only function that produces it.

Notice what `add_box` takes: a position, a size, a string, and two enums. Those enums and `struct grid_position` come from `box.c` and `drawables.c`, which `console.c` includes in its public part- which is exactly the rule from the box chapter, that if a public function takes something, the type has to be in the public part so one include is enough to use the component. `main.c` includes `console.c` and gets everything it needs to call `add_box`, including `top` and `bottom` and the colour names, without including anything else.

And notice what it does *not* take: a box. `main.c` cannot make a box, cannot hold one, and has no way to reach one after it is added. It asks the console for a box at a position and the console handles the rest.

### rows and cols

```c
		static struct console_size get_size(void){
			CONSOLE_SCREEN_BUFFER_INFOEX sbInfo={ .cbSize = sizeof(sbInfo) };
			GetConsoleScreenBufferInfoEx( GetStdHandle(STD_OUTPUT_HANDLE), &sbInfo );
			return (struct console_size){ .rows = sbInfo.dwMaximumWindowSize.X ,.cols =  sbInfo.dwMaximumWindowSize.Y };
		}
```

One thing to be aware of when reading the rest of the file: `rows` is the X extent and `cols` is the Y extent, which is the opposite of what those two words usually suggest. It is consistent everywhere- `draw_all( consoleBuffer, get_size().rows, get_size().cols )` passes them into `draw_all`'s `width` and `height` in that order- but read `rows` as "how wide" and `cols` as "how tall" and the arithmetic will make sense.

### One frame at a time

```c
		static HANDLE tick( struct timer* this ){
			CHAR_INFO consoleBuffer[ (get_size().rows+1) * (get_size().cols+1) ];
			memset( consoleBuffer, 0, sizeof(consoleBuffer) );
			drawable_objects.draw_all( consoleBuffer, get_size().rows, get_size().cols );
			WriteConsoleOutputW(
				GetStdHandle( STD_OUTPUT_HANDLE ),
				consoleBuffer,
				(COORD){ get_size().rows, get_size().cols },
				(COORD){ 0, 0 },
				&(SMALL_RECT){ 0, 0, get_size().rows , get_size().cols  }
			);
			return this;
		}
```

The buffer is a variable length array sized from the console every frame, so resizing the window is picked up on the next tick with no reallocation and nothing to free. It lives on the stack, which is why `timer.c` asks `CreateThread` for a 16 megabyte stack.

The order matters and is the whole point of drawing into a buffer rather than to the console: clear, let every drawable contribute, then hand the finished frame over in a single `WriteConsoleOutputW`. Nothing is ever shown half drawn, and there is no flicker, because the console is never in an intermediate state- it goes from one complete frame to the next.

This is also the only place that decides what a frame *is*. `draw_all` does not clear and does not present; it only composes. If I wanted to draw into two buffers alternately, or to something that is not a console at all, this function is the only one that would change.

### run

```c
		static void run(void){
			is_drawing = true;
			SetWindowPlacement( GetConsoleWindow(), &(WINDOWPLACEMENT){ sizeof(WINDOWPLACEMENT), .showCmd = SW_SHOWMAXIMIZED } );
			SetConsoleOutputCP(65001);
			WaitForSingleObject( timers.new( tick, (ms){1} ), INFINITE );
		}
```

Maximize the window, switch the output code page to 65001 so the box drawing characters `┌ ─ ┐ │ └ ┘` in `box.c` come out as themselves, start a 1 ms timer on `tick`, and block on it forever. `run` never returns, which is why `main` has nothing after it.

### add_box, where the wiring happens

```c
		static void add_box(
			struct grid_position grid_position, ... ){
  			drawable_objects.add(
				grid_position,
				&box.new(
					(struct box_arguments){
						.text = wstring(boxtext),
						.color = console_color,
						.grid_size = { .width = grid_width, .height = grid_height },
						.text_placement = text_placement
					}
				)->drawable
			);
		}
```

This is the composition, and it is a single expression.

`box.new` returns a `struct box_interface*`- the public face of a box, with the private `struct box` behind it that nobody out here can see. `->drawable` reaches the drawable that `box_interface` starts with, and its address is what the collection wants. So the box is created, immediately handed to the collection, and the pointer is never stored anywhere else. Nothing after this line can reach that box except through `draw_all`.

`wstring(boxtext)` is the widening conversion from `string.c`, and because `struct wstring` is a value the box gets its own copy of the text- `main.c` built that string on its stack in `boxname` and can let it go.

The four things this function knows: that a box exists, that it can be drawn, that the collection accepts drawables, and that text needs widening. That is the entire coupling in this program, and it is confined to one function.

```c
		typeof(console) console = {
			.run = run,
			.add_box = add_box,
			.get_size = get_size
		};
```

The vtable filled at compile time, with `typeof(console)` picking up the type- including the `const`- from the extern declaration in the public part.

### A note on the two threads

`drawtick` on one thread calls `add_box`, which appends to the collection. `tick` on the other thread walks that same collection in `draw_all`. There is no lock between them, and `append` reallocates and frees the old buffer, so this is racy in principle- the draw thread can be walking a buffer that the add thread is about to free.

It survives here because the tick rates are slow in machine terms and the window between the two is small, but it survives by luck rather than by design. The place to fix it would be in `drawables.c`, since the collection is the shared thing and both threads only reach it through `add` and `draw_all`.

One detail that is *not* luck: the `last` pointer inside `this()` in `array.c` is declared `static __thread`. Each thread gets its own, so the add thread calling `this(&drawables)` cannot disturb what the draw thread has stored. Without that one keyword the two threads would overwrite each other's notion of which array is being operated on.
