#include "stdafx.h"
#include "console.c"
#include "timer.c"
	static struct string boxname( int16_t x, int16_t y, int count ){
		return str(str(str(x),",",str(y)),"-",str(count));
	}
	static HANDLE drawtick( struct timer* ){   
		const auto height = 8;
		const auto width = height * 2;
		static __thread _Atomic bool down = true;
		static __thread _Atomic bool left_to_right = true;
		static __thread _Atomic bool boxcount = 0;
		static __thread _Atomic int16_t x = 0;
		static __thread _Atomic int16_t y = 0;  

		console.add_box( 
			(struct grid_position){ .x = x, .y = y },
			height,
			width, 
			boxname( x, y, ++boxcount ),
			down ? top : bottom,
			1 + ( x % 15 ) 
		);	
		if( x == console.get_size().rows - width ){
			left_to_right = ! left_to_right;
		}
		down ? y++ : y--;
		left_to_right ? x++ : x--; 
		x = x % ( console.get_size().rows - width ); 
		if( ( y <= 0 ) || ( y >= console.get_size().cols - height ) ){
			down = ! down;
			x = x + width; 
		} 		
		return 0;
	} 
	int main( int argc, char* argv[] ){   
		timers.new( drawtick, 2 );
		console.run(); 
	}  
