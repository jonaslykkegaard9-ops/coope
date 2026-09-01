### string.c

A value type string, and the machinery to build one out of anything.

The first thing to notice is that this file is inverted compared to every other component:

```c
#include "stdafx.h"
#if __INCLUDE_LEVEL__  != 0
#	ifndef charbuf
#		define charbuf charbuf
```

Everywhere else I write `#if __INCLUDE_LEVEL__ == 0` to mark the private part. Here the whole file is wrapped in the opposite test, so `string.c` only exists when it is *included*. Compile it directly and you get an empty translation unit.

That is intentional- there is nothing here that needs to exist once in the program. Every function is `static inline` and every type is a value type, so each file that includes it gets its own copy and the compiler is free to inline all of it away. There is no state to own, so there is no reason to have a private part at all. `string.c` is closer to a header than to a component, and the include level test says so out loud.

### The string itself

```c
		struct nstring{ char array[ ARRSIZE ]; };
		struct wstring{ wchar_t array[ ARRSIZE/2 ]; };
		struct string{
			union{
				char array[ ARRSIZE ];
				struct nstring nstring;
				struct wstring wstring;
			} ;
		};
```

`ARRSIZE` is 512, from `stdafx.h`. A `struct string` is 512 bytes and nothing else- no pointer, no length, no allocation. The union gives three views of the same bytes: as a plain `char` array, as a narrow string, or as 256 wide characters, which is the same 512 bytes again.

This is the whole reason strings can be passed and returned by value everywhere. In `main.c`:

```c
	static struct string boxname( int16_t x, int16_t y, int count ){
		return str(str(str(x),",",str(y)),"-",str(count));
	}
```

That returns a string by value, out of a function, with no allocation and nothing to free. And `box_arguments` can hold a `struct wstring text` as a member rather than a pointer, which means a box owns its own text outright- there is no dangling pointer to worry about when the caller's buffer goes away, and `box.new` copying `arguments` into the box copies the text with it.

The cost is that every string is 512 bytes whether it holds five characters or five hundred, and that nothing here bounds checks- `string()` uses `strcpy` and the concatenating overloads use `strcat`. For box titles built out of a few numbers that is a trade I am happy with, but it is a deliberate trade and not an oversight.

`wstring()` walks the narrow string and widens each character into the wide view. That is the conversion `console.add_box` does when it hands text to the box, because the console buffer is `CHAR_INFO` and wants `WCHAR`.

### str, and letting the type pick the format string

This is the part I actually wanted out of this file. In c you normally have to tell `printf` what you are giving it, and if you get the format specifier wrong nothing tells you until it misbehaves at runtime. With `overload` we can make the type choose:

```c
#		define MAKE_STR( TYPE, FSTR )static overload inline struct string str( TYPE in ){ \
			struct string buffer; \
			snprintf( buffer.array, ARRSIZE, FSTR, in ); \
			return buffer; \
		}
#		define FSTR( $ )\
				$(_Bool              , "%d"   ) \
				$(char               , "%c"   ) \
				$(signed char        , "%hhd" ) \
				...
				$(void*				 , "%p"   )
				FSTR( MAKE_STR )
```

Same `$` pattern as the `TYPES` list in `array.c`- `FSTR` is a list that applies whatever macro you hand it to every entry, and here we hand it `MAKE_STR` to generate one `str` overload per builtin type, each one carrying the correct format specifier for its own type.

The result is that `str(x)` is correct for any scalar in the language, and the format string never appears at a call site. You cannot pass an `int` where the format says `%s`, because you never write the format at all- the compiler picks the overload by the argument type and the format comes attached to it.

### Concatenation by overload

```c
				inline static overload struct string str( struct string a, struct string b ){
					struct string buffer={0};
					strcat( buffer.array, a.array );
					strcat( buffer.array, b.array );
					return buffer;
				}
```

Then the same for `char*` and `struct string` in every combination, and the three argument versions just fold into the two argument ones:

```c
				inline static overload struct string str( char* a, struct string b, char* c ){ return str( str( str(a), str(b) ), str(c) ); }
```

So `str` is simultaneously "convert this to a string" and "join these into a string", chosen by arity and type. That is what lets `boxname` in `main.c` read as a single expression mixing two `int16_t`, an `int` and two literals, without a single format string or temporary buffer.

### CAT and STR

```c
#	define _CAT($0,$1,...,$512,...) $0##$1##$2##...##$512
#	define CAT(...)_CAT( __VA_ARGS__,,,,,, ...512 empty arguments... )
```

`CAT` pastes up to 512 tokens into one identifier. It works by padding the call with a long tail of empty arguments so `_CAT` always receives a full parameter list, and pasting an empty argument contributes nothing- so `CAT(a,b)` and `CAT(a,b,c,d)` both come out as just the tokens joined.

It looks absurd written out, but it is generated once and never read again. `test.c` is what needs it, to manufacture a unique function name per test out of `__COUNTER__`.

```c
#	define _STR( ... )#__VA_ARGS__
#	define STR( ... )_STR( __VA_ARGS__ )
```

The usual two step stringize. Going through `_STR` means the argument gets macro expanded before it is turned into text, which is what makes `assert` able to print the expression that failed as the reader wrote it.

### log

```c
#	define logline( ... ) ( print(str(__FILE__)), print("."), print(str(__func__)), print(": "), xprint_(__VA_ARGS__,"\n",,,,,, ) )
#	define xprint_b( ... ) __VA_OPT__(  logline( str(__VA_ARGS__) ) , )
```

`log` takes up to twelve items of any type, runs each through `str` and prints them with the file and function name in front. `__VA_OPT__` is what makes the unused slots disappear- an empty argument expands to nothing at all rather than to an empty print.

Because everything goes through `str`, you log values, not formatted text- there is no format string to keep in sync with the arguments.
