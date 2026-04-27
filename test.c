#ifndef testarrayof
#	define testarrayof testarrayof  
#	include "stdafx.h" 
#	include "array.c" 
#	include "string.c" 
#	if __INCLUDE_LEVEL__ == 0 
#		ifdef TEST
#			define run_test_print(...)	__attribute__((constructor(__COUNTER__+1000))) static inline void CAT(autorun,__COUNTER__,__VA_ARGS__)(void){\
											puts("Running test "STR(__VA_ARGS__)":");\
											void CAT(test,__VA_ARGS__)(void);\
											CAT(test,__VA_ARGS__)();\
										}
#			define test(...)			run_test_print(__VA_ARGS__)\
										static inline void CAT(test,__VA_ARGS__)(void)

#			define assert(...)({ auto result = (__VA_ARGS__); if( result == 0 ){ print( str(STR(__VA_ARGS__)" failed")); __debugbreak();}})
#		else
#			define test(...)__if_exists(__notexisting)
#			define assert(...)
#		endif

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

		test(array){ 
			auto memory = array_of("jonas"); 
			assert( strcmp( this(memory)->append( "jonas" )->value, "jonasjonas" ) == 0 );
			
			auto int_arr = array_of( (int[]){0,1,2} );
			assert( int_arr->element_count == 3 );
			assert( int_arr->value[0] == 0 );
			assert( int_arr->value[1] == 1 );
			assert( int_arr->value[2] == 2 );
			
			auto int_arr_appended_twice = this(int_arr)->append( ((int[]){3,4,5}) );
			assert( int_arr_appended_twice->value[0] == 0 );
			assert( int_arr_appended_twice->value[1] == 1 );
			assert( int_arr_appended_twice->value[2] == 2 );
			assert( int_arr_appended_twice->value[3] == 3 );
			assert( int_arr_appended_twice->value[4] == 4 ); 
			assert( int_arr_appended_twice->value[5] == 5 ); 
		}
#	endif
#endif