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