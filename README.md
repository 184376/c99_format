# c99 printf like format
<br>
c99 standard (ISO/IEC 9899:1999) printf like format function with custom output callback,
it take a format string and optional arguments and produce a formatted sequence of characters for output callback
<br>
#include <stdint.h>
<br>
#include <stdarg.h>
<br>
typedef char ichar;
<br>
typedef intptr_t(*OslFormatWriteFunc)(void* arg, const ichar* sz, intptr_t len);
<br>
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* arg, const ichar* format, va_list argptr);
<br>
 writefunc:the output callback
 <br>
 arg:argument for writefunc
 <br>
 format:<a href="https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160">format string </a>
 <br>
 argptr:from va_start
 <br>
 return:
<br>
  
<br> 
