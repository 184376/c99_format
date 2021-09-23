# c99 printf like format
<br>
like printf，but use custom output callback, 
<br>

 ```c 

#include <stdarg.h>   
#include <stdint.h>

 typedef char ichar;

typedef intptr_t(*OslFormatWriteFunc)(void* arg, const ichar* sz, intptr_t len);
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* arg, const ichar* format, va_list argptr);

```

<br>
## Parameters 
* writefunc *
 <br>
The output callback
 <br>
 *arg*
 <br>
 Argument for output callback
 <br>
 <font color="green">&nbsp;&nbsp;format</font>:format string you can see on<a href="https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160"> MSDN</a> or <a href="https://linux.die.net/man/3/printf"> Liunx Man</a>
 <br>
 *argptr*
 <br>
Pointer to list of arguments.from va_start()
 <br>
##Return Value
  Return the number of characters written, not including the terminating null character, or a negative value if an output error occurs. 
<br>
##Example

 ```c  
struct vsnformat_data {
  ichar* buffer;
  intptr_t count;
};

static intptr_t _osl_vsnformat_write(struct vsnformat_data* arg, const ichar* sz, intptr_t len) {
  if (len >= arg->count) {
    return -1;
  }
  memcpy(arg->buffer, sz, len * sizeof(ichar));
  arg->buffer += len;
  arg->count -= len;
  return len;
}

int osl_vsnprintf(ichar* buffer, size_t count, const ichar* format, va_list ap) {
  int rv;
  struct vsnformat_data data;
  if (buffer == NULL) {
    return (int)osl_vformat(NULL, &data, format, ap);
  }

  data.buffer = buffer;
  data.count = count;
  rv = (int)osl_vformat((OslFormatWriteFunc)_osl_vsnformat_write, &data, format, ap);

  if (rv < 0) {
    buffer[0] = 0;
  }
  else {
    if (rv == count) {
      rv = (int)(count - 1);
    }
    buffer[rv] = 0;
  }
  return rv;
}

```
 
<br> 
