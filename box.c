#include "stdafx.h"
#ifndef box
#	define box box  
#	include "drawables.c" 
#	include "string.c" 
	enum text_placement{ center, top, bottom };
	enum console_color{
		BLACK		= 0, 
		DARKBLUE	= 1, 
		DARKGREEN	= FOREGROUND_GREEN,
		DARKCYAN	= FOREGROUND_GREEN | FOREGROUND_BLUE, 
		DARKRED		= FOREGROUND_RED,
        DARKMAGENTA = FOREGROUND_RED | FOREGROUND_BLUE, 
		DARKYELLOW	= FOREGROUND_RED | FOREGROUND_GREEN, 
		DARKGRAY	= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        GRAY		= FOREGROUND_INTENSITY,
		BLUE		= FOREGROUND_INTENSITY | FOREGROUND_BLUE,
		GREEN		= FOREGROUND_INTENSITY | FOREGROUND_GREEN,
		CYAN		= FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
        RED			= FOREGROUND_INTENSITY | FOREGROUND_RED, 
		MAGENTA		= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
		YELLOW		= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
        WHITE		= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE 
	};	
	typedef union{ 
		struct box_arguments{
			union{
				struct grid_size{ 
					int16_t width; 
					int16_t height;
				}grid_size; 
				struct grid_size; 
			};
			enum text_placement text_placement;
			enum console_color color;
			struct wstring text;
		}box_arguments;
		struct box_arguments;
	}box_arguments;
	typedef union{
		struct box_interface{
			drawable;
		}box_interface;
		struct box_interface;
	}box_interface;	
	extern const struct{ 
		 struct box_interface* (*new)( struct box_arguments arguments );
	}box; 
#	if __INCLUDE_LEVEL__ == 0 
		struct box{ box_interface; box_arguments; }
		static struct box_interface* new( struct box_arguments arguments ){
			struct box* newbox = calloc( sizeof(struct box), 1 ); 
			newbox->box_arguments = arguments; 

			union transparent converter{ struct box* box; struct drawable* interface;};
			void draw( union converter this, struct _CHAR_INFO* const consoleBuffer, int16_t rownr );
			newbox->draw = &draw;

			newbox->drawable.object = &newbox->drawable; 
			return &newbox->box_interface;
		}   
		static overload void set_char( CHAR_INFO* console_char, WCHAR UnicodeChar, enum console_color color ){
			console_char->Attributes = color;
			console_char->Char.UnicodeChar = UnicodeChar;
		}
		void draw( struct box* const this, CHAR_INFO* const consoleBuffer, int16_t rownr ){
			if( rownr > this->height ){ 
				return; 
			} 
			if( rownr == 0 || rownr == this->height ){
				for( int16_t charnr = 0; charnr < this->width; ++charnr ){
					set_char( &consoleBuffer[ charnr ], L"─"[0],this->color );
				}
				if( (( this->text_placement == top ) && (rownr == 0)) || ( (this->text_placement == bottom) && (rownr == this->height))){
					auto center = this->width / 2;
					if( wcslen(this->text.array) ){
						auto textstart = center - ( wcslen(this->text.array) / 2 );
						for( auto pos = textstart; pos <= ( textstart + wcslen(this->text.array) ); ++pos ){
							set_char( &consoleBuffer[ pos ], this->text.array[ pos - textstart ] , this->color );
						}
					}
				}
				if( rownr == 0 ){
					set_char( &consoleBuffer[ 0 ], L"┌"[0],this->color );
					set_char( &consoleBuffer[ this->width ],  L"┐"[0],this->color );
				}else{
					set_char( &consoleBuffer[ 0 ], L"└"[0],this->color );
					set_char( &consoleBuffer[ this->width ],  L"┘"[0],this->color );
				}
			}else{
				set_char( &consoleBuffer[ 0 ], L"│"[0],this->color );
				set_char( &consoleBuffer[ this->width ], L"│"[0] ,this->color);
				if( this->text_placement == center && ( rownr == ( this->height / 2) ) ){
					auto center = ( this->width / 2 );
					auto textstart = center - ( wcslen(this->text.array) / 2 );
					for( auto pos = textstart; pos <= ( textstart + wcslen(this->text.array) ); ++pos ){
						set_char( &consoleBuffer[ pos ], this->text.array[ pos - textstart ], this->color );
					}
				}
			}
		}
		typeof(box) box = { new };
#	endif
#endif
