#include "stdafx.h"
#ifndef array
#	define array array
#	include "drawables.c"
	typedef unsigned long long unsigned_long_long;
	typedef signed char signed_char;
	typedef unsigned char unsigned_char;
	typedef long long long_long;
	typedef unsigned short unsigned_short;
	typedef unsigned int unsigned_int;
	typedef unsigned long unsigned_long;
	typedef unsigned long long unsigned_long_long;
	typedef long double long_double;
#	define TYPES($)	$(void)$(_Bool)$(char)$(signed_char)$(unsigned_char)$(short)$(int)$(long)$(long_long)\
					$(unsigned_short)$(unsigned_int)$(unsigned_long)$(float)$(double)$(long_double)$(drawable_in_grid)
#	define typeid(TYPE) TYPE##_typeid,
		enum typeid{ TYPES(typeid) };
#	undef typeid  	 
	typedef union transparent{
#		define ANYTYPE(TYPE)struct array_of_##TYPE* array_of_##TYPE;
			TYPES(ANYTYPE);
#		undef ANYTYPE
}	any_array_ptr; 
#	define DEREF_UNLESS_VOID(...)typeof(_Generic(((__VA_ARGS__*){}),void*:(void*){},void**:(typeof(__VA_ARGS__*)){},default:(typeof(__VA_ARGS__)){}))
#	define THIS(TYPE)\
		overload inline struct TYPE##_id{ char typenr[ TYPE##_typeid ]; }typeid( TYPE* type );\
		overload inline struct array_of_##TYPE{\
			union{ TYPE* value; };\
			enum typeid typeid; \
			uint64_t element_count; \
			uint64_t element_size;\
			union{\
				struct vtable_##TYPE{\
					struct array_of_##TYPE* (*append)( struct array_of_##TYPE* );\
				}vtable; \
				struct vtable_##TYPE;\
			};\
		}* this( struct array_of_##TYPE* array ){\
			overload struct array_of_##TYPE* append(struct array_of_##TYPE* );\
			static __thread struct array_of_##TYPE* last = 0; \
			if(array){ \
				array->vtable.append = append;\
				array->typeid = sizeof( typeid( (TYPE*){} ) );\
				array->element_size = sizeof( TYPE ) ;\
				last = array;\
			}\
			return last;\
		}\
		static overload struct array_of_##TYPE array_of( DEREF_UNLESS_VOID(TYPE*) type ); \
		static overload struct TYPE##_id typeid( struct array_of_##TYPE* type );
		TYPES(THIS)
#	undef THIS 
#	undef DEREF_UNLESS_VOID
#	if __INCLUDE_LEVEL__ == 0  
		overload any_array_ptr append( struct array_of_char* stem, struct array_of_char* to_append ){ 
			auto total_size = stem->element_count + to_append->element_count  - 2 ;
			char* new = calloc( stem->element_size , total_size );
			memcpy( new, stem->value, stem->element_count  );
			memcpy( new + stem->element_count-1  , to_append->value, to_append->element_count  );
			auto prev = stem->value;
			stem->value = new;
			stem->element_count = total_size;
			free(prev);
			return (any_array_ptr)stem;
		}
		overload any_array_ptr append( any_array_ptr stem, any_array_ptr to_append ){ 
			auto total_size = stem.array_of_void->element_count + to_append.array_of_void->element_count;
			void* new = calloc( stem.array_of_void->element_size , total_size );
			memcpy( new, stem.array_of_void->value, stem.array_of_void->element_count  * stem.array_of_void->element_size );
			memcpy( new + ( stem.array_of_void->element_count * stem.array_of_void->element_size  ), to_append.array_of_void->value , to_append.array_of_void->element_count * to_append.array_of_void->element_size );
			auto prev = stem.array_of_void->value;
			stem.array_of_void->element_count = total_size;
			stem.array_of_void->value =	new;
			free(prev);
			return (any_array_ptr)stem;
		}  
#		define APPEND(TYPE)overload struct array_of_##TYPE* append( struct array_of_##TYPE* to_append ){ return append( this((struct array_of_##TYPE*){0}), to_append ).array_of_##TYPE ;;}
			TYPES(APPEND)
#		undef APPEND
#	else
#		define typeid(TYPE)				sizeof( typeid( ( typeof( (TYPE) )* )0 ) )  
#		define append(array )			append( array_of( array ) )
#		define element_count(...)		( (sizeof(__VA_ARGS__)) / (element_size(__VA_ARGS__)) )
#		define element_size(...)		sizeof(__VA_ARGS__[0])
#		define ptr_element_first(...)	(&__VA_ARGS__[0])
#		define allocate_for(...)		( calloc( element_size(__VA_ARGS__), element_count(__VA_ARGS__) ) )
#		define on_heap(...)				( memcpy( allocate_for(__VA_ARGS__), __VA_ARGS__, ( element_count(__VA_ARGS__) * element_size(__VA_ARGS__) ) ) )
#		define array_of( ... )			(&( typeof( array_of( ptr_element_first(__VA_ARGS__) ) ) ){   \
											 .value = on_heap (__VA_ARGS__),						  \
											 .element_count = element_count(__VA_ARGS__),			  \
											 .element_size = element_size(__VA_ARGS__),				  \
											 .typeid = typeid(*ptr_element_first(__VA_ARGS__))		  \
										} )	 
#	endif
#endif
