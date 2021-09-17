# c99 printf like format
useage:
typedef intptr_t(*OslFormatWriteFunc)(void* arg, const ichar* sz, intptr_t len);
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* arg, const ichar* format, va_list argptr);

