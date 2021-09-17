#pragma once
#define  _CRT_SECURE_NO_WARNINGS 
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>   
#include <stdint.h>
#include <assert.h>

#ifndef FALSE
#define FALSE 0
#endif // FALSE

#ifndef TRUE
#define TRUE 1
#endif // TRUE

typedef int ibool;
typedef char ichar;

typedef intptr_t(*OslFormatWriteFunc)(void* arg, const ichar* sz, intptr_t len);
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* arg, const ichar* format, va_list argptr);

