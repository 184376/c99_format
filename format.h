 
 
#include <stdarg.h>
#include <stdint.h>

typedef intptr_t(*OslFormatWriteFunc)(void* userData, const char* sz, intptr_t len);
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* userData, const char* format, va_list argptr);

