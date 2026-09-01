### drawable.c and drawables.c

These two files belong together: `drawable.c` is the interface a component implements when it wants to be drawn, and `drawables.c` is the collection that keeps track of every such object and paints them into the console buffer.

Notice that `drawable.c` has no private part at all- there is no `__INCLUDE_LEVEL__` block in it. There is nothing to implement, the file is pure interface, so the whole file is the public part.

```c
#include "stdafx.h"
#ifndef drawable
#	define drawable drawable
	typedef union{
		struct drawable{
			struct drawable* object;
			union{
				struct drawable_vtable{
					void (*draw)( struct drawable* this, struct _CHAR_INFO* const consoleBuffer, int16_t rownr );
				}drawable_vtable;
				struct drawable_vtable;
			};
		}drawable;
		struct drawable;
	}drawable;
#endif
```

This is the same union, named struct, unnamed struct pattern from the box- the members are reachable both as `drawable.drawable_vtable.draw` and directly as `drawable.draw`, and because it is a union they are the very same bytes.

The interface is two things only:
- `object`- the pointer that gets passed back as the first argument when we call draw. This is the compensation for c not having a thiscall calling convention, exactly like the `this` function does it for the array.
- `draw`- draw *one row* of yourself into the buffer you are handed.

That is the whole contract. A drawable never learns where it is on the screen, and it never learns how big the console is. It is told: here is a pointer into the buffer where your row starts, and this is which of your own rows it is. Everything else is the collection's problem.

`box.c` implements this by simply writing `drawable;` as the first member of its public `struct box_interface`- so a `struct box*` starts with a `struct drawable`, and that is what makes the transparent union trick in `box.new` legal.

### drawables.c

The public part first:

```c
	typedef	struct drawable_in_grid{
		union{
			struct grid_position{
				int16_t x;
				int16_t y;
			}grid_position;
			struct grid_position;
		};
		drawable;
	}drawable_in_grid;
	extern struct{
		void (*draw_all)( CHAR_INFO* const consolebuffer, int16_t width, int16_t height );
		void (*add)( struct grid_position grid_position, const struct drawable* const toadd );
	}drawable_objects;
```

A `drawable_in_grid` is just a drawable that has been given a place to live. Because both the position and the drawable are flattened in, one element gives us `element.x`, `element.y`, `element.object` and `element.draw` all directly- while `grid_position` is still a nameable type on its own, which is what lets `console.add_box` and `main.c` pass a position around as a value.

`grid_position` being declared here rather than in the box is deliberate- position is not a property of a box, it is a property of a box *in this collection*. A box has a size, not a location.

Then the usual extern struct that the private part fills in at compile time.

### The include cycle

```c
#	if __INCLUDE_LEVEL__ == 0
#		include "array.c"
		static struct array_of_drawable_in_grid drawables;
```

Worth stopping at. `array.c` includes `drawables.c` at the top of *its* public part- because `drawable_in_grid` is one of the entries in the `TYPES` list, so the array component needs the type to generate `struct array_of_drawable_in_grid` for it. And here `drawables.c` includes `array.c` right back.

That would be a loop, except the `#ifndef` guard on each file makes the second visit expand to nothing. And notice the direction: `drawables.c` only reaches for `array.c` from *inside* its private part. The public interface of drawables does not depend on arrays at all- the array is an implementation detail of the collection, so it stays behind the `__INCLUDE_LEVEL__` fence and nobody who includes `drawables.c` pays for it.

The collection itself is one static, file scope array. There is exactly one list of drawables in the program.

### add

```c
		static void add( struct grid_position grid_position, const struct drawable* const toadd ){
			this(&drawables)->append((
				(struct drawable_in_grid[]){ { {grid_position}, {*toadd} } }
			));
		}
```

The compound literal builds a one element array of `drawable_in_grid` right there in the argument, and `append` is the macro from the include side of `array.c`, so it becomes `append( array_of( ... ) )`- the literal is measured for element count and element size, copied to the heap, and appended. `this(&drawables)` first stores the array we are operating on so that the append we call actually lands on the right object.

`*toadd` copies the drawable by value into the element- but `object` inside it still points at the original box on the heap. So the collection owns a copy of the *vtable and the self pointer*, while all of the box' actual state stays where `box.new` put it. That is why `console.add_box` can pass `&box.new(...)->drawable` and then forget about the box entirely.

### draw_all

```c
		static void draw_all( CHAR_INFO* const consolebuffer, int16_t width, int16_t height ){
			if( this(&drawables)->element_count == 0 ){ return; }
			for( int16_t row = 0; row <= height; row++ ){
				for( uint32_t i = 0; i < this(&drawables)->element_count; i++ ){
					if( row >= this(&drawables)->value[i].y ){
						this(&drawables)->value[i].draw(
							this(&drawables)->value[i].drawable.object,
							&consolebuffer[ (width * row) + this(&drawables)->value[i].x ],
							row - this(&drawables)->value[i].y
						);
					}
				}
			}
		}
```

Row major, one scanline at a time: for every row of the console we ask every object that has already started to contribute its share of that row. The three arguments are exactly the three pieces of the contract:

- `value[i].drawable.object`- the object pointer we stored in `box.new`, handed back as the `this`.
- `&consolebuffer[ (width * row) + x ]`- not the start of the buffer, but the start of *that object's* row. The console buffer is one flat array, so `width * row` picks the row and `+ x` slides right to the object's column. Everything the drawable writes at index 0 lands where it should, and the drawable never has to know the console width.
- `row - y`- the row number translated into the object's own coordinates. The box asks `rownr == 0` for its top border and `rownr == this->height` for its bottom one, and never has to think about screen coordinates at all.

The `row >= y` test is what makes a box that starts further down simply not participate until we reach it, and the box returns early on `rownr > this->height` once we have gone past its bottom.

Drawing in element order means later objects overwrite earlier ones where they overlap- a painter's algorithm, with insertion order as the z order. In `main.c` boxes are added continuously by the timer, so the newest box is always the one on top.

The collection never clears anything. That is `console.c`'s job- its own tick memsets a fresh `consoleBuffer`, calls `draw_all` into it, and hands the finished buffer to `WriteConsoleOutputW` in a single call. So there is no flicker and no partial frame: every frame is composed completely before any of it is shown.

### What the pattern buys

`drawables.c` includes `drawable.c` and nothing else in its public part. It has never heard of a box, a string or a console. `box.c` implements draw and never hears about the collection. `console.c` is the only file that knows both exist, and it is the one that wires them together in `add_box`.

Adding a second kind of drawable- a line, a menu, a progress bar- means writing one more component that puts `drawable;` first in its interface and hands `&thing.new(...)->drawable` to `drawable_objects.add`. Nothing in `drawables.c` changes, and nothing in it needs to be told the new type exists.
