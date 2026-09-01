### test.c

A unit test component that costs nothing when it is not enabled, needs no registration list, and needs no test runner.

```c
#ifndef testarrayof
#	define testarrayof testarrayof
#	include "stdafx.h"
#	include "array.c"
#	include "string.c"
#	if __INCLUDE_LEVEL__ == 0
#		ifdef TEST
```

Same include level fence as everywhere else, with a second condition on top: the tests only exist when the file is compiled directly *and* `TEST` is defined. Two switches, so tests never leak into a build that did not ask for them.

### Tests that register themselves

```c
#			define run_test_print(...)	__attribute__((constructor(__COUNTER__+1000))) static inline void CAT(autorun,__COUNTER__,__VA_ARGS__)(void){\
											puts("Running test "STR(__VA_ARGS__)":");\
											void CAT(test,__VA_ARGS__)(void);\
											CAT(test,__VA_ARGS__)();\
										}
#			define test(...)			run_test_print(__VA_ARGS__)\
										static inline void CAT(test,__VA_ARGS__)(void)
```

`test(name)` expands into two things: a constructor function, and the opening of a definition of `testname`. The braces the reader writes after `test(array)` become that function's body. So this:

```c
		test(array){
			...
		}
```

is a complete function definition plus a constructor that calls it, out of what reads like a block with a label on it.

`__attribute__((constructor))` makes a function run before `main`. The priority argument controls the order, and `__COUNTER__+1000` gives each test a number one higher than the last, so the tests run in the order they appear in the file. `__COUNTER__` increments every time it is read, which is also why the generated `autorun` name is unique without me having to name it- and that is what `string.c`'s `CAT` is for, pasting `autorun`, the counter value and the test name into one identifier.

The result is that there is no list of tests anywhere. Writing `test(something)` is the whole registration. Nothing has to be updated when a test is added, and there is no way to add a test and forget to run it.

The forward declaration of `CAT(test,__VA_ARGS__)` inside the constructor is what lets the constructor come first and still call a function that is defined afterwards.

### Compiling to nothing

```c
#		else
#			define test(...)__if_exists(__notexisting)
#			define assert(...)
#		endif
```

Without `TEST`, `assert` becomes empty and `test` becomes `__if_exists(__notexisting)`- a block that is only compiled if a symbol by that name exists, and nothing is called `__notexisting`. So the braces following `test(array)` are swallowed whole and the test body never reaches the compiler. Not compiled out with `#if`, but discarded by the language, which means the tests can live in a normal source file with no preprocessor scaffolding wrapped around each one.

### assert

```c
#			define assert(...)({ auto result = (__VA_ARGS__); if( result == 0 ){ print( str(STR(__VA_ARGS__)" failed")); __debugbreak();}})
```

A statement expression, so the condition is evaluated exactly once into `result` no matter how many times the macro body mentions it- a plain macro that used `__VA_ARGS__` twice would run a function call in the condition twice.

On failure it prints the expression as written, using `STR` from `string.c`, and then `__debugbreak()`- it breaks into the debugger at the failing line with the whole state still live, rather than aborting and leaving you with a message. `str` and `print` are the same ones from `string.c`, so an assert message costs no format string either.

### Testing the drawable interface

```c
		void testdraw( struct drawable* this, CHAR_INFO* const consoleBuffer, int16_t  ){
			assert(this);
			assert(consoleBuffer);
		}

		test(drawable_objects){
			struct drawtest{
				drawable;
			}obj = {{ .object = &obj.drawable, .draw = &testdraw }};

			drawable_objects.add(
				(struct grid_position){0,0},
				obj.object
			);
			assert(
				drawable_objects.draw_all( &(CHAR_INFO){}, 2, 2 ),
				1
			);
		}
```

This is worth reading as documentation of the interface as much as a test, because it shows the whole cost of implementing a drawable: one member `drawable;` and one initializer pointing `object` at itself. No allocation, no constructor, no interface registration- `struct drawtest` is a local on the stack and the collection accepts it exactly like a box.

It also demonstrates the thing the box chapter was about, from the other side. `testdraw` takes a `struct drawable*` and the box's `draw` takes a `struct box*`, and both are valid `draw` implementations, because what the interface stores is the transparent union's worth of signatures rather than one fixed one.

The third parameter of `testdraw` is unnamed- it is the row number, which this implementation does not care about, and leaving the name off says that rather than leaving an unused variable behind.

One honest note about that last assert: `draw_all` returns void, and `assert` evaluates `(__VA_ARGS__)`, so `assert( draw_all(...), 1 )` is a comma expression that always comes out as 1. It checks that the call is reachable and does not crash, not that it drew anything. To assert on the result you would need the drawable to record that it was called- a counter in `struct drawtest` that `testdraw` increments, checked afterwards.

### The array tests

```c
		test(array){
			auto memory = array_of("jonas");
			assert( strcmp( this(memory)->append( "jonas" )->value, "jonasjonas" ) == 0 );
			...
```

These are the ones shown in the array chapter. The char case exists specifically to prove the special cased `append` for `array_of_char` overwrote the terminating null instead of leaving it in the middle- `"jonasjonas"` and not `"jonas\0jonas"`. The int case then proves the general append kept every element intact, and that the two are dispatched apart correctly by type.

`auto` here is `__auto_type` from `stdafx.h`. The array types are generated per contained type and named `struct array_of_int` and so on, so `auto` saves writing out a name that the macro produced.
