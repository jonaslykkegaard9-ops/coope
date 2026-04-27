#include "stdafx.h"
#ifndef drawables
#	define drawables drawables 
#	include "drawable.c"
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
#	if __INCLUDE_LEVEL__ == 0
#		include "array.c"
		static struct array_of_drawable_in_grid drawables;		
		static void add( struct grid_position grid_position, const struct drawable* const toadd ){
			this(&drawables)->append(( 
				(struct drawable_in_grid[]){ { {grid_position}, {*toadd} } } 
			));
		}
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
		typeof(drawable_objects) drawable_objects = { .add = add, .draw_all = draw_all };
#	endif 
#endif
