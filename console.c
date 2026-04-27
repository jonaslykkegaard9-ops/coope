#include "stdafx.h"
#if !defined(console)
#	define console console 
#	include "drawables.c"
#	include "box.c"
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
#	if __INCLUDE_LEVEL__ == 0  
#		include "timer.c"  
		static bool is_drawing = false;
		static struct console_size get_size(void){
			CONSOLE_SCREEN_BUFFER_INFOEX sbInfo={ .cbSize = sizeof(sbInfo) };
			GetConsoleScreenBufferInfoEx( GetStdHandle(STD_OUTPUT_HANDLE), &sbInfo ); 
			return (struct console_size){ .rows = sbInfo.dwMaximumWindowSize.X ,.cols =  sbInfo.dwMaximumWindowSize.Y };
		}
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
		static void run(void){
			is_drawing = true;
			SetWindowPlacement( GetConsoleWindow(), &(WINDOWPLACEMENT){ sizeof(WINDOWPLACEMENT), .showCmd = SW_SHOWMAXIMIZED } );
			SetConsoleOutputCP(65001);
			WaitForSingleObject( timers.new( tick, (ms){1} ), INFINITE );
		}
		static void add_box( 
			struct grid_position grid_position, 
			int16_t grid_height,
			int16_t grid_width,
			struct string boxtext, 
			enum text_placement text_placement,
			enum console_color console_color 
		){	
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
		typeof(console) console = {
			.run = run,
			.add_box = add_box,
			.get_size = get_size
		};
#	endif
#endif
