#if !defined(main)  
#	define main main 
#	define ARRSIZE 512
#	define auto __auto_type
#	define overload __attribute__((overloadable))
#	define transparent __attribute__((__transparent_union__)) 

#	pragma clang diagnostic ignored "-Wmicrosoft-anon-tag" 
#	pragma clang diagnostic ignored "-Wunused-function"
#	define _CRT_SECURE_NO_WARNINGS 1  
#	define STRICT
#		include <Windows.h> 
#	undef STRICT

#	include <stdio.h>
#	include <stdlib.h> 
#	include <wchar.h> 
#	include <stdint.h> 

	typedef enum{ false, true }bool; 

#	undef interface 
#	undef THIS 
#endif