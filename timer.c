#include "stdafx.h"
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
#	if __INCLUDE_LEVEL__ == 0  
#		include "string.c"  
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
		static HANDLE new( timer_function function, ms tickrate ){
			timer new_timer = calloc( sizeof(struct timer), 1 );
			new_timer->timer_function = function;
			new_timer->tickrate = tickrate;
			new_timer->active = true;
			return tick( new_timer );
		}  
		typeof(timers) timers = { .new = new }; 
#	endif
#endif
