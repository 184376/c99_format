# c99 printf like format
<br>
useage:
<br>
typedef intptr_t(*OslFormatWriteFunc)(void* arg, const ichar* sz, intptr_t len);
<br>
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* arg, const ichar* format, va_list argptr);
<br>
see https://www.man7.org/linux/man-pages/man3/printf.3.html
or https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160
