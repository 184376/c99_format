
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>  
#include "format.h"
#ifndef FALSE
#define FALSE 0
#endif // FALSE

#ifndef TRUE
#define TRUE 1
#endif // TRUE

struct vsnformat_data {
    char* buffer;
    intptr_t count;
};

static intptr_t _osl_vsnformat_write(struct vsnformat_data* arg, const char* sz, intptr_t len) {
    if (len >= arg->count) {
        return -1;
    }
    memcpy(arg->buffer, sz, len * sizeof(char));
    arg->buffer += len;
    arg->count -= len;
    return len;
}

int osl_vsnprintf(char* buffer, size_t count, const char* format, va_list ap) {
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
 
 
int osl_snprintf(char* buffer, intptr_t count, const char* format, ...) {
    int rv;
    struct vsnformat_data data;
    va_list argptr;
    data.buffer = buffer;
    data.count = count;
    va_start(argptr, format);
    rv = (int)osl_vformat((OslFormatWriteFunc)_osl_vsnformat_write, &data, format, argptr);
    va_end(argptr);
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

//test test test test

static void _osl_make_format_string(char* format, const char* flags, int width, int precision, char fmt) {
    char* start = format;
    *format = '%';
    format++;
    if (flags) {
        size_t len = strlen(flags);
        memcpy(format, flags, len * sizeof(char));
        format += strlen(flags);
    }
    if (width >= 0) {
        _itoa_s(width , format, 256 - (format - start), 10);
        format +=strlen(format);
    }
    if (precision >= 0) {
        *format = '.';
        format++;
        _itoa_s(precision, format,  256 - (format - start),10);
        format += strlen(format);
    }
    *format = fmt;
    format++;
    *format = 0;
    assert((format - start) < 256);
}

int sys_vsnprintf(char* buffer, intptr_t count, const char* format, va_list ap) {
    int rv;
#ifdef _OS_WINDOWS
    rv = _vsnprintf_s(buffer, count, count, format, ap);
    if (rv <= 0) {
        buffer[0] = 0;
    }
    else if (rv == count) {
        rv = (int)(count - 1);
        buffer[rv] = 0;
    }
#else // !_OS_WINDOWS
    rv = vsnprintf(buffer, count, format, ap);
#endif // _OS_WINDOWS
    return rv;
}

int _osl_cmp_snprintf(const char* format, ...) {
    char buffer1[512];
    char buffer2[512];
    va_list argptr1;
    va_list argptr2;
    va_list argptr3;
    va_start(argptr1, format);
    va_copy(argptr2, argptr1);
    va_copy(argptr3, argptr1);
    sys_vsnprintf(buffer1, 512, format, argptr1);
    osl_vsnprintf(buffer2, 512, format, argptr2);
    va_end(argptr1);
    va_end(argptr2);

    if (strcmp(buffer1, buffer2) == 0) {
        va_end(argptr3);
        return TRUE;
    }
    printf("fmt:'%s',sys:%zd,%zd\n'%-s'\n'%-s'\n", format, strlen(buffer1), strlen(buffer2), buffer1, buffer2);
    //for debug code trace.
    osl_vsnprintf(buffer2, 512, format, argptr3);
    va_end(argptr3);
    return FALSE;
}

const char* fmt_flags = " 0#-+";
const char** all_fmt_flags = NULL;
int all_fmt_flag_count = 0;
const int max_width = 30;
const int max_precision = 40;

static int  test_build_all_fmt_flags() {
    if (all_fmt_flags != NULL) {
        return all_fmt_flag_count;
    }
    int fmt_flags_len = 5;
    all_fmt_flag_count = 1 + 5 + 10 + 10 + 5 + 1;
    int buf_size = (all_fmt_flag_count + 1) * 6;
    char* buf = (char*)malloc(buf_size);
    if (buf == NULL) {
        return 0;
    }
    char* pos = buf;
    const char** all_flags = (char**)malloc(sizeof(char*) * all_fmt_flag_count);
    if (all_flags == NULL) {
        return 0;
    }
    all_fmt_flags = all_flags;
    all_flags[0] = NULL;
    int n = 1;
    for (int i = 0; i < 5; i++) {
        all_flags[n] = pos;
        n++;
        *pos = fmt_flags[i];
        pos++;
        *pos = 0;
        pos++;
    }

    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            all_flags[n] = pos;
            *pos = fmt_flags[i];
            pos++;
            *pos = fmt_flags[j];
            pos++;
            *pos = 0;
            pos++;
            n++;
        }
    }
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            for (int k = j + 1; k < 5; k++) {
                all_flags[n] = pos;
                *pos = fmt_flags[i];
                pos++;
                *pos = fmt_flags[j];
                pos++;
                *pos = fmt_flags[k];
                pos++;
                *pos = 0;
                pos++;
                n++;
            }
        }
    }

    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            for (int k = j + 1; k < 5; k++) {
                for (int l = k + 1; l < 5; l++) {
                    all_flags[n] = pos;
                    *pos = fmt_flags[i];
                    pos++;
                    *pos = fmt_flags[j];
                    pos++;
                    *pos = fmt_flags[k];
                    pos++;
                    *pos = fmt_flags[l];
                    pos++;
                    *pos = 0;
                    pos++;
                    n++;
                }
            }
        }
    }
    all_flags[n] = fmt_flags;
    n++;
    assert(n == all_fmt_flag_count);
    assert((pos - buf) <= buf_size);
    // all_fmt_flag_count = n;
    return all_fmt_flag_count;
}

const char* int_formats = "oxXidu";
void _osl_printf_test_int(int value) {
    printf("test int %d\n", value);
    char format[256]; 
    const char* fpos = int_formats;
    while (*fpos) {
        for (int i = 0; i < all_fmt_flag_count; i++) {
            _osl_make_format_string(format, all_fmt_flags[i], -1, -1, *fpos);
            _osl_cmp_snprintf(format, value);
        }
        fpos++;
    }
    fpos = int_formats;
    while (*fpos) {
        for (int i = 0; i < all_fmt_flag_count; i++) {
            for (int width = 0; width < max_width; width++) {
                for (int precision = 0; precision < max_precision; precision++) {
                    _osl_make_format_string(format, all_fmt_flags[i], width, precision, *fpos);
                    _osl_cmp_snprintf(format, value);
                }
            }
        }
        fpos++;
    }
}

const char* double_formats = "aefg";
void _osl_printf_test_double(double value) {
    printf("test double %g\n", value);
    char format[256]; 
    const char* fpos = double_formats;
    while (*fpos) {
        for (int i = 0; i < all_fmt_flag_count; i++) {
            _osl_make_format_string(format, all_fmt_flags[i], -1, -1, *fpos);
            _osl_cmp_snprintf(format, value);
        }
        fpos++;
    }
    fpos = double_formats;
    while (*fpos) {
        for (int i = 0; i < all_fmt_flag_count; i++) {
            for (int width = 0; width < max_width; width++) {
                for (int precision = 0; precision < max_precision; precision++) {
                    _osl_make_format_string(format, all_fmt_flags[i], width, precision, *fpos);
                    _osl_cmp_snprintf(format, value);
                }
            }
        }
        fpos++;
    }
    //test_calc_f64(value);
}
 
void osl_format_test_impl() { 
    double float_val[] = {
       0,
   #ifdef NAN
      NAN,
      -NAN,
      INFINITY,
      -INFINITY,
   #endif//NAN
   #ifdef DBL_MAX
      DBL_TRUE_MIN,
      FLT_TRUE_MIN,
      DBL_MAX,
      DBL_MIN,
      DBL_EPSILON,
      FLT_MAX,
      FLT_MIN,
      FLT_EPSILON,
   #endif//NAN
      1.5e-40,
      1.5e-200,
      1.0e+40,
      1.0e-40,
      1.0 / 3.0,
      2.0 / 3.0,

      -4.136,
      -134.52,
      -5.04030201,
      -3410.01234,
      -999999.999999,
      -913450.29876,
      -913450.2,
      -91345.2,
      -9134.2,
      -913.2,
      -91.2,
      -9.2,
      -9.9,
      4.136,
      134.52,
      5.04030201,
      3410.01234,
      999999.999999,
      913450.29876,
      913450.2,
      91345.2,
      9134.2,
      913.2,
      91.2,
      9.2,
      9.9,
      9.96,
      9.996,
      9.9996,
      9.99996,
      9.999996,
      9.9999996,
      9.99999996,
      0.99999996,
      0.99999999,
      0.09999999,
      0.00999999,
      0.00099999,
      0.00009999,
      0.00000999,
      0.00000099,
      0.00000009,
      0.00000001,
      0.0000001,
      0.000001,
      0.00001,
      0.0001,
      0.001,
      0.01,
      0.1,
      1.0,
      1.5,
      -1.5,
      -1.0,
      -0.1,
    };

    if (0 == test_build_all_fmt_flags()) {
        printf("test_build_all_fmt_flags no memory.");
        return;
    }
      
    for (size_t i = 0; i < sizeof(float_val) / sizeof(float_val[0]); i++) {
        _osl_printf_test_double(float_val[i]);
    }

    int int_val[] = {
  #ifdef INT_MAX
      INT_MAX,
  #endif	/* LONG_MAX */
  #ifdef INT_MIN
      INT_MIN,
  #endif	/* LONG_MIN */
      - 91340,
      91340,
      341,
      134,
      0203,
      -1,
      1,
      0
    };

    for (size_t i = 0; i < sizeof(int_val) / sizeof(int_val[0]); i++) {
        _osl_printf_test_int(int_val[i]);
    }
}

void osl_format_test() {
    osl_format_test_impl();
    printf("\ntest finished.\n");
}


