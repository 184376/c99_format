
 
#include <math.h>
#include <float.h> 
#include <errno.h> 
#include "format.h"

#if defined(_WIN32) || defined(_WIN64)
#define _OSL_WINDOWS
#elif defined(__linux__)  
#define _OLS_LINUX
#elif  defined(__APPLE__) &&  defined(__MACH__)
#define _OSL_MAC
#if defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#define _OSL_MAC_IOS
#endif
#define _OSL_MAC_OSX
#else 
#error Unknown platform detected.
#endif

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

typedef struct vasnformat_data vasnformat_data;
struct vasnformat_data {
  intptr_t count; 
  ichar* data; 
  intptr_t capacity;
};

static intptr_t vasnformat_data_ensure_cap(vasnformat_data* self, intptr_t new_count) {
  if (new_count <= self->capacity) {
    return 1;
  }
#ifdef _grow_fixed_count
  intptr_t new_cap = self->_capacity;
  while (new_cap < new_count) {
    new_cap += _grow_fixed_count;
  }
#undef _grow_fixed_count
#else //!_grow_fixed_count
  intptr_t new_cap = self->capacity << 1;
  if (new_cap == 0)
    new_cap = 16;
  while (new_cap < new_count) {
    new_cap <<= 1;
  }
#endif // #ifdef _grow_fixed_count

  ichar* data = (ichar*)realloc(self->data, new_cap * sizeof(ichar));
  if (NULL == data) {
    return 0;
  }
  self->data = data;
  self->capacity = new_cap;
  return 1;
}

static intptr_t osl_vasprintf_write(vasnformat_data* arg, const ichar* sz, intptr_t len) {
  if (!vasnformat_data_ensure_cap(arg, len))
    return -1;
  ichar*buf = arg->data + arg->count; 
  memcpy(buf,sz,len*sizeof(ichar));
  return len;
}

int osl_vasprintf(ichar** strp, const ichar* format, va_list argptr) {
  int rv;
  struct vasnformat_data data = {0,NULL,0};
  rv = (int)osl_vformat((OslFormatWriteFunc)osl_vasprintf_write, &data, format, argptr); 
  if (rv < 0) {
    *strp = NULL;
    if (data.data)
      free(data.data);
  }
  else {
    if (!vasnformat_data_ensure_cap(&data, 1)) {
      if (data.data)
        free(data.data);
      return -1;
    } 
    data.data[data.count]=0; 
    *strp = data.data;  
  }
  return rv;
}

int osl_snformat(ichar* buffer, intptr_t count, const ichar* format, ...) {
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
 
#define _OSL_NUMBER_BUFFER_LENGTH 511
#define _OSL_NUMBER_DEFAULT_PRECISION 6
typedef struct OslFormatter OslFormatter;
struct OslFormatter {
  int width;
  int precision;
  ibool left_align;
  ibool with_sign;
  ibool padding_zero;
  ibool prefix_blank;
  ibool alternate_form;
  ibool specifieris_upper;
   
  OslFormatWriteFunc writefunc;
  void* arg;
  intptr_t count;

  ichar tempbuf[_OSL_NUMBER_BUFFER_LENGTH + 1];
  //ichar cvtbuf[_OSL_NUMBER_BUFFER_LENGTH + 1];
};

static intptr_t osl_vformat_null_write(void* arg, const ichar* sz, intptr_t len) {
  return len;
}
 
static intptr_t _osl_vformat_impl(OslFormatter* formatter, const ichar* szformat, va_list argptr);

/// <summary>
/// Writes the C string pointed by format 
/// not support 'S' and long double
/// </summary>
/// <param name="writefunc"></param>
/// <param name="arg"></param>
/// <param name="szformat">A format specifier follows this prototype
/// %[flags][width][.precision][length]specifier
/// </param>
/// <param name="argptr"></param>
/// <returns></returns>
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* arg, const ichar* szformat, va_list argptr) {
  OslFormatter formatter;
  formatter.count = 0;
  formatter.writefunc = writefunc;
  formatter.arg = arg;
  if (writefunc == NULL)
    formatter.writefunc = osl_vformat_null_write;
  return _osl_vformat_impl(&formatter, szformat, argptr);
}

static const ichar _osl_digits_upper[] = "0123456789ABCDEF";
static const ichar _osl_digits_lower[] = "0123456789abcdef";
#define osl_int_str_buf_length 64
#define osl_define_vformat_itoa(name,type,base)\
static ichar* name(type value, ichar* buf) {\
  ichar tmp[osl_int_str_buf_length];\
  int n = 0;\
  do {\
    tmp[n] = '0'+(value % base);\
    value /= base;\
    n++;\
  } while (value != 0);\
  n--;\
  assert(n < osl_int_str_buf_length);\
  ichar* pos = buf;\
  for (int i = n; i >= 0; i--) {\
    *pos = tmp[i];\
    pos++;\
  }\
  *pos = 0;\
  return pos;\
}

#define osl_define_vformat_itoa16(name,type)\
static ichar* name(type value, ichar* buf, ibool is_upper) {\
  const ichar* digits;\
  ichar tmp[osl_int_str_buf_length];\
  digits = is_upper ? _osl_digits_upper : _osl_digits_lower;\
  int n = 0;\
  do {\
    tmp[n] = digits[value % 16];\
    value /= 16;\
    n++;\
  } while (value != 0);\
  n--;\
  assert(n < osl_int_str_buf_length);\
  ichar* pos = buf;\
  for (int i = n; i >= 0; i--) {\
    *pos = tmp[i];\
    pos++;\
  }\
  *pos = 0;\
  return pos;\
}
 
osl_define_vformat_itoa(_osl_vformat_utoa10, uint32_t, 10)
osl_define_vformat_itoa(_osl_vformat_ultoa10, uint64_t, 10)
osl_define_vformat_itoa16(_osl_vformat_ultoa16, uint64_t)

static ibool _osl_vformat_append(OslFormatter* formatter, const ichar* start, intptr_t len) {
  if (len <= 0)
    return TRUE;
  if (formatter->writefunc(formatter->arg, start, len) < 0)
    return FALSE;
  formatter->count += len;
  return TRUE;
}

static ibool _osl_vformat_append_nchar(OslFormatter* formatter, ichar ch, intptr_t count) {
  if (count <= 0)
    return TRUE;
  for (intptr_t i = 0; i < count; i++) {
    if (formatter->writefunc(formatter->arg, &ch, 1) < 0)
      return FALSE;
  }
  formatter->count += count;
  return TRUE;
}
 
static ibool _osl_vformat_append_with_prefix(OslFormatter* formatter, const ichar* prefix, int prefixLen, const ichar* digits, intptr_t len) {

  intptr_t padding_zero_count = formatter->precision - len;
  if (padding_zero_count < 0) {
    padding_zero_count = 0;
  }
  else if (padding_zero_count > 0) {
    if (prefix && prefix[prefixLen - 1] == '0')
      padding_zero_count--;
  }
  intptr_t padding_black_count = formatter->width - len - prefixLen - padding_zero_count;
  if (padding_black_count < 0)
    padding_black_count = 0;
  //right align
  if (!formatter->left_align) {
    if (formatter->padding_zero) {
      padding_zero_count += padding_black_count;
      padding_black_count = 0;
    }
    if (!_osl_vformat_append_nchar(formatter, ' ', padding_black_count))
      return FALSE;
    if (prefix && !_osl_vformat_append(formatter, prefix, prefixLen)) {
      return FALSE;
    }

    if (!_osl_vformat_append_nchar(formatter, '0', padding_zero_count))
      return FALSE;
  }
  else {
    if (prefix && !_osl_vformat_append(formatter, prefix, prefixLen)) {
      return FALSE;
    }
    if (!_osl_vformat_append_nchar(formatter, '0', padding_zero_count))
      return FALSE;
    padding_zero_count = 0;
  }

  if (!_osl_vformat_append(formatter, digits, len))
    return FALSE;

  if (formatter->left_align) {
    if (!_osl_vformat_append_nchar(formatter, ' ', padding_black_count + padding_zero_count))
      return FALSE;
  }

  return TRUE;
}

static ibool _osl_vformat_append_with_prefix_double(OslFormatter* formatter, const ichar* prefix, int prefixLen, const ichar* digits, intptr_t len) {
  
  intptr_t padding_zero_count = 0;
  intptr_t padding_black_count = formatter->width - len - prefixLen;
  if (padding_black_count < 0)
    padding_black_count = 0;
  //right align
  if (!formatter->left_align) {
    if (formatter->padding_zero) {
      padding_zero_count = padding_black_count;
      padding_black_count = 0;
    }
    if (!_osl_vformat_append_nchar(formatter, ' ', padding_black_count))
      return FALSE;
    if (prefix && !_osl_vformat_append(formatter, prefix, prefixLen)) {
      return FALSE;
    }
    if (!_osl_vformat_append_nchar(formatter, '0', padding_zero_count))
      return FALSE;
  }
  else if (prefix && !_osl_vformat_append(formatter, prefix, prefixLen)) {
    return FALSE;
  }

  if (!_osl_vformat_append(formatter, digits, len))
    return FALSE;

  if (formatter->left_align) {
    if (!_osl_vformat_append_nchar(formatter, ' ', padding_black_count))
      return FALSE;
  }

  return TRUE;
}

static ibool _osl_vformat_append_integer(OslFormatter* formatter, unsigned int base, ibool neg, const ichar* digits, intptr_t len) {

  const ichar* prefix = NULL;
  int prefixLen = 0;

  if (base == 10) {
    if (neg) {
      prefix = "-";
      prefixLen = 1;
    }
    else if (formatter->prefix_blank) {
      prefix = " ";
      prefixLen = 1;
    }
    else if (formatter->with_sign) {
      prefix = "+";
      prefixLen = 1;
    }
  }
  else if (formatter->alternate_form) {
    //For o conversions, the first character of the output string 
    //is made zero(by prefixing a 0 if it was not zero already).
    //For xand X conversions, a nonzero result has the string "0x" (or "0X" for X conversions) prepended to it
    if (base == 16) {
      if (digits != NULL && digits[0] != '0') {
        prefixLen = 2;
        if (formatter->specifieris_upper) {
          prefix = "0X";
        }
        else {
          prefix = "0x";
        }
      }
    }
    else {
      if (digits == NULL || digits[0] != '0') {
        prefix = "0";
        prefixLen = 1;
      }
    }
  }

  return _osl_vformat_append_with_prefix(formatter, prefix, prefixLen, digits, len);
}
 
static ibool _osl_vformat_uint64(OslFormatter* formatter, uint64_t value, unsigned int base, ibool neg) {
  const ichar* digits;
  ichar* buf = formatter->tempbuf;
  ichar* pos;
  intptr_t len;
  //If both the converted value and the precision are ?0? the conversion results in no characters.
  if (value == 0 && formatter->precision == 0)
    return _osl_vformat_append_integer(formatter, base, neg, NULL, 0);
  if (formatter->specifieris_upper)
    digits = _osl_digits_upper;
  else
    digits = _osl_digits_lower;
  pos = buf + _OSL_NUMBER_BUFFER_LENGTH;

  if (base == 10) {
    for (;;) {
      *pos = '0'+value % 10;
      value /= 10;
      if (value == 0)
        break;
      pos--;
    }
  }
  else if (base == 16) {
    for (;;) {
      *pos = digits[value % 16];
      value /= 16;
      if (value == 0)
        break;
      pos--;
    }
  }
  else if (base==8) {
    for (;;) {
      *pos = '0'+value % 8;
      value /= 8;
      if (value == 0)
        break;
      pos--;
    }
  }
  else {
    return FALSE;
  }
  len = buf + _OSL_NUMBER_BUFFER_LENGTH - pos + 1;
  return _osl_vformat_append_integer(formatter, base, neg, pos, len);
}

static ibool _osl_vformat_int64(OslFormatter* formatter, int64_t value, unsigned int base) {
  uint64_t val;
  ibool neg;
  if (value < 0) {
    val = -value; neg = TRUE;
  }
  else {
    val = value; neg = FALSE;
  }

  return _osl_vformat_uint64(formatter, val, base, neg);
}

static ibool _osl_vformat_append_string(OslFormatter* formatter, const ichar* sz, intptr_t len) {
  if (len < formatter->width) {
    if (!formatter->left_align) {
      intptr_t delta = formatter->width - len;
      while (delta) {
        delta--;
        if (!_osl_vformat_append(formatter, " ", 1))
          return FALSE;
      }
    }
  }
  if (!_osl_vformat_append(formatter, sz, len))
    return FALSE;
  if (len < formatter->width) {
    if (formatter->left_align) {
      intptr_t delta = formatter->width - len;
      while (delta) {
        delta--;
        if (!_osl_vformat_append(formatter, " ", 1))
          return FALSE;
      }
    }
  }
  return TRUE;
}

static ibool _osl_vformat_append_double(OslFormatter* formatter, ibool neg, const ichar* digits, intptr_t len) {
  const ichar* prefix = NULL;
  int prefixLen = 0;

  if (neg) {
    prefix = "-";
    prefixLen = 1;
  }
  else if (formatter->prefix_blank) {
    prefix = " ";
    prefixLen = 1;
  }
  else if (formatter->with_sign) {
    prefix = "+";
    prefixLen = 1;
  }

  return _osl_vformat_append_with_prefix_double(formatter, prefix, prefixLen, digits, len);
}

static ibool _osl_vformat_string(OslFormatter* formatter, const ichar* sz) {
  intptr_t len;
  if (sz == NULL) {
    sz = "(null)";
  }
  len = strlen(sz);
  if (formatter->precision >= 0 && len > formatter->precision) {
    len = formatter->precision;
  }
  return _osl_vformat_append_string(formatter, sz, len);
}

static ichar* _osl_vformat_remove_trailing_zero_guard(const ichar* guard, const ichar* pos, ibool keepDot) {
  while (pos > guard && (*pos == '0')) {
    pos--;
  }
  if (!keepDot && *pos == '.') {
    return (ichar*)pos;
  }
  return (ichar*)(pos + 1);
}
 
/*
For a conversion, the double argument is converted to hexadecimal notation
(using the letters abcdef) in the style [-]0xh.hhhhp¡À; for A conversion the prefix 0X,
the letters ABCDEF, and the exponent separator P is used.
There is one hexadecimal digit before the decimal point,
and the number of digits after it is equal to the precision.
The default precision suffices for an exact representation of the value
if an exact representation in base 2 exists and otherwise is sufficiently
large to distinguish values of type double. The digit before
the decimal point is unspecified for nonnormalized numbers,
and nonzero but otherwise unspecified for normalized numbers.
*/

typedef double float64_t;
union ui64_f64 { uint64_t ui; float64_t f; };
#define signF64UI( a ) ((int_fast16_t) ((uint64_t) (a)>>63))
#define expF64UI( a ) ((int32_t) ((a)>>52) & 0x7FF)
#define fracF64UI( a ) ((a) & UINT64_C( 0x000FFFFFFFFFFFFF ))
 
static intptr_t _osl_hcvt_ieee754d64(OslFormatter* formatter, ichar* buf,
  intptr_t  buf_count,
  double  value,
  int precision, int* prefix_len) {
    
  *prefix_len = 2; 
  ichar* pos = buf;
   
  if (value >= 0) {
    if (formatter->with_sign) {
      *pos = '+';
      pos++;
      (*prefix_len)++;
    }
    else if (formatter->prefix_blank) {
      *pos = ' ';
      pos++;
      (*prefix_len)++;
    }
  }
  else {
    value = -value;
    *pos = '-';
    pos++;
    (*prefix_len)++;
  }
  *pos = '0';
  pos++;
  *pos = *pos = formatter->specifieris_upper ? 'X' : 'x';
  pos++;

  union ui64_f64 ua;
  ua.f = value;
  uint64_t ui = ua.ui;
  //uint64_t sign = signF64UI(ui);
  int32_t exponent = expF64UI(ui) - 1023;
  uint64_t frac = fracF64UI(ui);

  //*pexp = exp;
  //*pdotpos = 1;
  uint64_t mantissa;
  if (exponent == -1023) {
    //mantissa = frac + (UINT64_C(1) << 52);
    if (frac == 0) {
      exponent = 0;
      mantissa = 0;
    }
    else {
      mantissa = frac + (UINT64_C(1) << 52);
    }
  }
  else {
    mantissa = frac + (UINT64_C(1) << 52);
  }
  if (precision < 0)
    precision = 13;
  if (precision <= 12) {
#ifdef hcvt_round_away_from_zero
    //0x000FFFFFFFFFFFFF
    int shift = precision * 4;
    uint64_t round_add = (UINT64_C(0x0008000000000000) >> shift);
    assert(round_add > 0);
    mantissa += round_add;
#else //hcvt_round_near_even
    //0x000FFFFFFFFFFFFF 
    int shift = precision * 4;
#ifdef round_val_first
    uint64_t round_add = (UINT64_C(0x0008000000000000) >> shift);
    uint64_t round_val = (mantissa & (UINT64_C(0x000F000000000000) >> shift)) >> (48 - shift);
    if (round_val == 8) {
      uint64_t sticky_bit = (mantissa & (UINT64_C(0x0000FFFFFFFFFFFF) >> shift));
      if (sticky_bit == 0) {
        uint64_t guardval = (mantissa & (UINT64_C(0x00F0000000000000) >> shift)) >> (52 - shift);
        if (((guardval) % 2) == 1)
          mantissa += round_add;
      }
      else {
        mantissa += round_add;
      }
    }
    else if (round_val > 8) {
      mantissa += round_add;
    }
    else {//round_val < 8
    }
#elif defined(sticky_bit_first)
    uint64_t round_add = (UINT64_C(0x0008000000000000) >> shift);
    uint64_t sticky_bit = (mantissa & (UINT64_C(0x0000FFFFFFFFFFFF) >> shift));
    if (sticky_bit == 0) {
      uint64_t round_val = (mantissa & (UINT64_C(0x000F000000000000) >> shift)) >> (48 - shift);
      if (round_val == 8) {
        uint64_t guardval = (mantissa & (UINT64_C(0x00F0000000000000) >> shift)) >> (52 - shift);
        if (((guardval) % 2) == 1)
          mantissa += round_add;
      }
      else if (round_val > 8) {
        mantissa += round_add;
      }
      else {//round_val < 8
      }
    }
    else {
      mantissa += round_add;
    }
#else 
    uint64_t mask = (UINT64_C(0xFFFFFFFFFFFFFFFF) << (52 - shift));
    uint64_t mid_point = (mantissa & mask)
      | ((UINT64_C(0x00008000000000000) >> shift));
    if (mantissa == mid_point) {
      mantissa += (UINT64_C(0x0007000000000000) >> shift)
        + (mantissa & (UINT64_C(0x0010000000000000) >> shift));
    }
    else {
      mantissa += (UINT64_C(0x0008000000000000) >> shift);
    }
    mantissa &= mask;
#endif
#endif 
  }
  ichar tmp[32];
  _osl_vformat_ultoa16(mantissa, tmp,formatter->specifieris_upper);
  const ichar* tpos = tmp;

  //int part
  if (exponent == -1023) {
    *pos = '0';
    pos++;
    tpos++;
    exponent += 1;
  }
  else {
    *pos = *tpos;
    pos++;
    tpos++;
  }
  //precision--;

  if (precision > 0) {
    *pos = '.';
    pos++; 
    for (; (*tpos) && precision > 0; pos++, tpos++, precision--) {
       *pos = *tpos;
    }
    for (; precision > 0; pos++, precision--) {
       *pos = '0';
    } 
  }
  else if (formatter->alternate_form) {
    *pos = '.';
    pos++;
  }

  *pos = formatter->specifieris_upper ? 'P' : 'p';
  pos++;
  if (exponent >= 0) {
    *pos = '+';
    pos++;
  }
  else {
    exponent = -exponent;
    *pos = '-';
    pos++;
  }
  pos = _osl_vformat_utoa10(exponent, pos);
  *pos = 0;
  return pos-buf;
}

static intptr_t _osl_vformat_double_f(OslFormatter* formatter, ichar* buf, const ichar* cvt,
  int precision, ibool significant_precision, int dotpos, ibool trailingZeroes) {
  ichar* pos = buf;
  ichar* trailing_limit = buf;
  if (dotpos <= 0) {
    *pos = '0'; pos++;
  }
  else {
    for (int i = 0; i < dotpos; i++) {
      *pos = cvt[i]; pos++;
    }
    cvt += dotpos;
    trailing_limit += dotpos;
    if (significant_precision) {
      precision -= dotpos;
    }
  }

  if (formatter->alternate_form || (precision > 0)) {
    *pos = '.'; pos++;
  }

  if (precision > 0) {
    if (significant_precision) {
      for (int i = 0; i < -dotpos; i++) {
        *pos = '0'; pos++;
      }
    }
    else {
      for (int i = 0; (i < -dotpos) && precision > 0; i++, precision--) {
        *pos = '0'; pos++;
      }
    }
  }

  for (; (precision > 0) && (*cvt); precision--, cvt++) {
    *pos = *cvt; pos++;
  }
  if (trailingZeroes) {
    while (precision > 0) {
      *pos = '0'; pos++; precision--;
    }
  }
  else {
    pos = _osl_vformat_remove_trailing_zero_guard(trailing_limit, pos - 1, formatter->alternate_form);
  }

  return pos - buf;
}

static intptr_t _osl_vformat_double_e(OslFormatter* formatter, ichar* buf, const ichar* cvt,
  int precision, ibool significant_precision, int exponent, int dotpos, ibool trailingZeroes) {
  intptr_t len = _osl_vformat_double_f(formatter, buf, cvt, precision, significant_precision, dotpos, trailingZeroes);
  ichar* pos = buf + len;

  *pos = formatter->specifieris_upper ? 'E' : 'e';
  pos++;

  *pos = exponent >= 0 ? '+' : '-';
  pos++;
  if (exponent < 0)
    exponent = -exponent;
  int exponent_len = 2;
  if (exponent >= 100) {
    exponent_len = 3;
  }

  ichar* digits = pos + exponent_len - 1;

  for (;;) {
    *digits = exponent % 10 + '0';
    exponent /= 10; digits--;
    if (exponent == 0)
      break;
  }
  while (digits >= pos) {
    *digits = '0'; digits--;
  }
  *(pos + exponent_len) = 0;
  return pos + exponent_len - buf;
}

struct OslVFormatNan {
  int len;
  ichar value[9];
};

static ibool _osl_vformat_nan(OslFormatter* formatter, int negative,int classification,ichar specifier) {
  static const struct OslVFormatNan nan_value[] = {
     {3,"inf"},
     {3,"INF"}, 
     {3,"nan"},//Quiet NAN
     {3,"NAN"}, 
     {8,"nan(ind)"},//Indeterminate NAN
     {8,"NAN(IND)"},
     {9,"nan(snan)"},//Signaling NAN
     {9,"NAN(SNAN)"}, 
  };
    //formatter->padding_zero = FALSE;
    //formatter->with_sign = FALSE;
    //formatter->prefix_blank = FALSE;
    int nan_value_index = classification*2;
    if (formatter->specifieris_upper)
      nan_value_index++;
    const struct OslVFormatNan* nan = nan_value + nan_value_index;
    const ichar* digits = nan->value;
    int digist_len = nan->len;
    ichar* prefix = formatter->tempbuf;
    if (negative) {
      *prefix = '-';
      prefix++;
    }
    else if (formatter->prefix_blank) {
      *prefix = ' ';
      prefix++;
    }
    else if (formatter->with_sign) {
      *prefix = '+';
      prefix++;
    }
    int prefix_len = (int)(prefix - formatter->tempbuf);
 
    if (formatter->alternate_form && formatter->precision == 0 && specifier != 'g') { 
      if (negative) {
        if (specifier == 'a' ) {
          *prefix = '0';
          prefix++;
          *prefix = formatter->specifieris_upper ? 'X' : 'x';
          prefix++;
          prefix_len = (int)(prefix - formatter->tempbuf);
        }
        if (formatter->padding_zero) {
          int spcace = formatter->width - (prefix_len + nan->len + 1);
          while (spcace > 0) {
            *prefix = '0';
            prefix++;
            spcace--;
          }
          prefix_len = (int)(prefix - formatter->tempbuf);
        }

        *prefix = '.';
        prefix++;
      }
      else {
        *prefix = *digits;
        prefix++;
        *prefix = '.';
        prefix++;
        digits++;
        digist_len--;
      }
    } 
    *prefix = 0;
    prefix_len = (int)(prefix - formatter->tempbuf);
    if (prefix_len == 0)
      prefix = NULL;
    else {
      prefix = formatter->tempbuf;
    }
    formatter->padding_zero = FALSE;
    formatter->prefix_blank = FALSE;
    return _osl_vformat_append_with_prefix_double(formatter, prefix, prefix_len, digits, digist_len); 
   
}

extern int osl_convert_ieee754d64(ichar* buf, int buflen, double val, ichar specifier, int precision, int* pexp, int* pdotpos, ichar* pgret);

static ibool _osl_vformat_ieee754d64(OslFormatter* formatter, double value, ichar specifier) {
   
  union ui64_f64 ua;
  ua.f = value;
  uint64_t ui = ua.ui; 
  int32_t  exponent2 = expF64UI(ui);
  uint64_t frac = fracF64UI(ui);

  //  infinity 	inf             s = u; e = 2047; f = 0 
  //  Indefinite NaN 	nan(ind)  s = 1; e = 2047; f = .1000 -- 00
  //  Quiet NaN  nan            s = u; e = 2047; f = .1uuu -- uu 
  //  Signaling NaN 	nan(snan) s = u; e = 2047; f = .0uuu -- uu 0 (at least one of the u in f is nonzero)
   
  if (exponent2 == 2047) {
    int classification;
    int negative = signF64UI(ui);
    if (frac == 0) {//inf
      classification = 0;
    }
    else {//nan
#if 1
      //from msvc crt
      const uint64_t special_nan_mantissa_mask = (1ui64 << 51);
      if (negative && frac == special_nan_mantissa_mask) {
        //Indeterminate NAN
        classification = 2;
      }
      else if (frac & special_nan_mantissa_mask) {
        //Quiet NAN
        classification = 1;
      }
      else {
        //Signaling NAN
        classification = 3;
      }
#else 
      classification = 1;
#endif

    }  
    return _osl_vformat_nan(formatter, negative, classification, specifier);
  }  

  int precision;
  intptr_t len = 0;
  int exponent10;
  int dotpos;
  ichar gspecifier;
  ichar cvtbuf[_OSL_NUMBER_BUFFER_LENGTH + 1];

  switch (specifier) {
  case 'a':
    do{ 
      int prefix_len;
      precision = formatter->precision >= 0 ? formatter->precision : 13;
      len = _osl_hcvt_ieee754d64(formatter, formatter->tempbuf,
        _OSL_NUMBER_BUFFER_LENGTH, value, precision, &prefix_len);
      if (len <0)
        return FALSE; 
      if (len >= formatter->width || formatter->left_align)
        return _osl_vformat_append_string(formatter, formatter->tempbuf, len);
      else {
        if (formatter->padding_zero) {
          if (!_osl_vformat_append(formatter, formatter->tempbuf, prefix_len))
            return FALSE;
          if (!_osl_vformat_append_nchar(formatter, '0', formatter->width - len))
            return FALSE;
        }
        else {
          if (!_osl_vformat_append_nchar(formatter, ' ', formatter->width - len))
            return FALSE;
          if (!_osl_vformat_append(formatter, formatter->tempbuf, prefix_len))
            return FALSE;
        }
        return _osl_vformat_append(formatter, formatter->tempbuf + prefix_len, len - prefix_len);
      }
    }while (0);
  case 'e':
    precision = formatter->precision >= 0 ? formatter->precision : 6;
    osl_convert_ieee754d64(cvtbuf, _OSL_NUMBER_BUFFER_LENGTH, value, 'e',
      precision, &exponent10, &dotpos, NULL);
     
    len = _osl_vformat_double_e(
      formatter,
      formatter->tempbuf,
      cvtbuf,
      precision, FALSE, exponent10, dotpos, TRUE);
    break;
  case 'f':
    precision = (formatter->precision >= 0 ? formatter->precision : 6);
    osl_convert_ieee754d64(cvtbuf, _OSL_NUMBER_BUFFER_LENGTH, value, 'f', 
      precision, &exponent10, &dotpos, NULL);
    len = _osl_vformat_double_f(
      formatter,
      formatter->tempbuf,
      cvtbuf,
      precision, FALSE, dotpos, TRUE);
    break;
  case 'g':
    //The e format is used only when the exponent of the value is less than ?C4
    //or greater than or equal to the precision argument.Trailing zeros are truncated,
    //and the decimal point appears only if one or more digits follow it.

    //The precision specifies the number of significant digits. 
    //If the precision is missing, 6 digits are given;
    precision = (formatter->precision >= 0 ? formatter->precision : 6);
    if (precision == 0)
      precision = 1;
    osl_convert_ieee754d64(cvtbuf, _OSL_NUMBER_BUFFER_LENGTH, value, 'g', 
      precision, &exponent10, &dotpos, &gspecifier);
    //'#' For g and G
    //  conversions, trailing zeros are not removed from the
    //  result as they would otherwise be.
    // 
    //Style e is used if the exponent from its conversion is less than -4 
    // or greater than or equal to the precision.
    if (gspecifier == 'e') {//if ((exp < -4 || (exp >= precision))) {
      len = _osl_vformat_double_e(
        formatter,
        formatter->tempbuf,
        cvtbuf,
        precision, TRUE, exponent10, dotpos, formatter->alternate_form);
    }
    else {
      len = _osl_vformat_double_f(
        formatter,
        formatter->tempbuf,
        cvtbuf,
        precision, TRUE, dotpos, formatter->alternate_form);
    }   
    break;
  default:
    return FALSE;
  } 
  return _osl_vformat_append_double(formatter, value < 0, formatter->tempbuf, len);
}

static intptr_t _osl_vformat_impl(OslFormatter* formatter, const ichar* szformat, va_list argptr) {
  const ichar* psz;
  const ichar* start;

  psz = szformat;
  start = psz;

  for (;;) {

    ibool dot_without_precision;
    while (*psz && *psz != '%') {
      psz++;
    }

    if (*psz == 0) {
      if (!_osl_vformat_append(formatter, start, psz - start))
        return -1;
      break;
    }
    psz++;
    if (*psz == '%') {
      if (!_osl_vformat_append(formatter, start, psz - start))
        return -1;
      psz++;
      start = psz;
      continue;
    }

    if ((psz - start - 1) > 0
      && !_osl_vformat_append(formatter, start, psz - start - 1))
      return -1;

    formatter->left_align = FALSE;
    formatter->with_sign = FALSE;
    formatter->padding_zero = FALSE;
    formatter->prefix_blank = FALSE;
    formatter->alternate_form = FALSE;

    ibool run = TRUE;
    do {
      switch (*psz) {
      case '-':
        formatter->left_align = TRUE;
        psz++;
        break;
      case '+':
        formatter->with_sign = TRUE;
        psz++;
        break;
      case '0':
        formatter->padding_zero = TRUE;
        psz++;
        break;
      case ' ':
        formatter->prefix_blank = TRUE;
        psz++;
        break;
      case '#':
        formatter->alternate_form = TRUE;
        psz++;
        break;
      default:
        run = FALSE;
        break;
      }
    } while (run);
    //If  the 0 and -flags both appear, the 0 flag is ignored.
    if (formatter->left_align) {
      formatter->padding_zero = FALSE;
    }
    if (formatter->with_sign)
      formatter->prefix_blank = FALSE;

    formatter->width = -1;
    formatter->precision = -1;
    formatter->specifieris_upper = FALSE;
    //width
    if (*psz == '*') {
      formatter->width = va_arg(argptr, int);
      psz++;
    }
    else if (('0' <= *psz) && (*psz <= '9')) {
      formatter->width = 0;
      while (('0' <= *psz) && (*psz <= '9')) {
        formatter->width *= 10;
        formatter->width += (*psz - '0');
        psz++;
      }
    }

    //precision
    dot_without_precision = FALSE;
    if (*psz == '.') {
      psz++;
      if (*psz == '*') {
        formatter->precision = va_arg(argptr, int);
        psz++;
      }
      else if (('0' <= *psz) && (*psz <= '9')) {
        formatter->precision = 0;
        while (('0' <= *psz) && (*psz <= '9')) {
          formatter->precision *= 10;
          formatter->precision += (*psz - '0');
          psz++;
        }
      }
      else {
        dot_without_precision = TRUE;
      }
    }

    int arg_size = sizeof(int);

    switch (*psz) {
    case 'h':
      psz++;
      if (*psz == 'h') {
        psz++;
        arg_size = sizeof(char);
      }
      else {
        arg_size = sizeof(short);
      }
      break;
    case 'l':
      psz++;
      if (*psz == 'l') {
        arg_size = sizeof(int64_t);
        psz++;
      }
      else {
        arg_size = sizeof(int);
      }
      break;
    case 'I':
      psz++;
      if (*psz == '3' && *(psz + 1) == '2') {
        psz += 2;
        arg_size = sizeof(int);
      }
      else if (*psz == '6' && *(psz + 1) == '4') {
        psz += 2;
        arg_size = sizeof(int64_t);
      }
      else {
        arg_size = sizeof(void*);
      }
      break;
    case 'j':
      psz++;
      arg_size = sizeof(intmax_t);
      break;
    case 't':
    case 'z':
      psz++;
      arg_size = sizeof(void*);
      break;
    case 'L':
      psz++;
      assert(sizeof(double) == sizeof(long double));
      break;
    default:
      break;
    }

    uint64_t i_val;
    int64_t u_val;
    ichar char_val;
    switch (*psz) {
    case 'c':
      char_val = va_arg(argptr, int);
      if (!_osl_vformat_append(formatter, &char_val, 1))
        return -1;
      break;
    case 'd':
    case 'i':
      //If a precision is given with a numeric conversion(d, i, o,
      //  u, x, and X), the 0 flag is ignored.
      if (formatter->precision >= 0)
        formatter->padding_zero = FALSE;
      if (arg_size == sizeof(int)) {
        i_val = va_arg(argptr, int);
      }
      else if (arg_size == sizeof(int64_t)) {
        i_val = va_arg(argptr, int64_t);
      }
      else  if (arg_size == sizeof(short)) {
        i_val = (short)va_arg(argptr, int);
      }
      else if (arg_size == sizeof(char)) {
        i_val = (char)va_arg(argptr, int);
      }
      else {
        return -1;
      }

      if (!_osl_vformat_int64(formatter, i_val, 10))
        return -1;
      break;
    case 'u':
      //If a precision is given with a numeric conversion(d, i, o,
      //  u, x, and X), the 0 flag is ignored.
      if (formatter->precision >= 0)
        formatter->padding_zero = FALSE;
      formatter->with_sign = FALSE;
      formatter->prefix_blank = FALSE;
      if (arg_size == sizeof(int)) {
        u_val = va_arg(argptr, unsigned int);
      }
      else if (arg_size == sizeof(uint64_t)) {
        u_val = va_arg(argptr, uint64_t);
      }
      else  if (arg_size == sizeof(short)) {
        u_val = (unsigned short)va_arg(argptr, int);
      }
      else if (arg_size == sizeof(char)) {
        u_val = (unsigned char)va_arg(argptr, int);
      }
      else {
        return -1;
      }

      if (!_osl_vformat_uint64(formatter, u_val, 10, FALSE))
        return -1;
      break;
    case 'o':
      //If a precision is given with a numeric conversion(d, i, o,
      //  u, x, and X), the 0 flag is ignored.
      if (formatter->precision >= 0)
        formatter->padding_zero = FALSE;
      formatter->with_sign = FALSE;

      if (arg_size == sizeof(int)) {
        u_val = va_arg(argptr, unsigned int);
      }
      else if (arg_size == sizeof(uint64_t)) {
        u_val = va_arg(argptr, uint64_t);
      }
      else  if (arg_size == sizeof(short)) {
        u_val = (unsigned short)va_arg(argptr, int);
      }
      else if (arg_size == sizeof(char)) {
        u_val = (unsigned char)va_arg(argptr, int);
      }
      else {
        return -1;
      }

      if (!_osl_vformat_uint64(formatter, u_val, 8, FALSE))
        return -1;
      break;
    case 'X':
      formatter->specifieris_upper = TRUE;
    case 'x':
      //If a precision is given with a numeric conversion(d, i, o,
      //  u, x, and X), the 0 flag is ignored.
      if (formatter->precision >= 0)
        formatter->padding_zero = FALSE;
      formatter->with_sign = FALSE;

      if (arg_size == sizeof(int)) {
        u_val = va_arg(argptr, unsigned int);
      }
      else if (arg_size == sizeof(uint64_t)) {
        u_val = va_arg(argptr, uint64_t);
      }
      else  if (arg_size == sizeof(short)) {
        u_val = (unsigned short)va_arg(argptr, int);
      }
      else if (arg_size == sizeof(char)) {
        u_val = (unsigned char)va_arg(argptr, int);
      }
      else {
        return -1;
      }

      if (!_osl_vformat_uint64(formatter, u_val, 16, FALSE))
        return -1;
      break;
    case 'p':
      //Pointer address
#ifdef _OSL_WINDOWS
      formatter->specifieris_upper = TRUE;
#else // !_OSL_WINDOWS
      formatter->extra_flag = TRUE;
      formatter->specifieris_upper = FALSE;
#endif // _OSL_WINDOWS
      formatter->with_sign = FALSE;
      formatter->padding_zero = TRUE;
      if (sizeof(void*) == sizeof(uint64_t)) {
        formatter->width = 16;
        if (!_osl_vformat_uint64(formatter, va_arg(argptr, uint64_t), 16, FALSE))
          return -1;
      }
      else {
        formatter->width = 8;
        if (!_osl_vformat_uint64(formatter, va_arg(argptr, unsigned int), 16, FALSE))
          return -1;
      }
      break;
    case 'n':
      //Nothing printed.The corresponding argument must be a pointer to a signed int.
      //The number of characters written so far is stored in the pointed location.
      *va_arg(argptr, int*) = (int)formatter->count;
      break;
    case 'F':
      formatter->specifieris_upper = TRUE;
    case 'f':
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_osl_vformat_ieee754d64(formatter, va_arg(argptr, double), 'f'))
        return -1;
      break;
    case 'E':
      formatter->specifieris_upper = TRUE;
    case 'e':
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_osl_vformat_ieee754d64(formatter, va_arg(argptr, double), 'e'))
        return -1;
      break;
    case 'G':
      formatter->specifieris_upper = TRUE;
    case 'g':
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_osl_vformat_ieee754d64(formatter, va_arg(argptr, double), 'g'))
        return -1;
      break;
    case 'A'://Hexadecimal floating point, uppercase
      formatter->specifieris_upper = TRUE;
    case 'a'://Hexadecimal floating point, lowercase
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_osl_vformat_ieee754d64(formatter, va_arg(argptr, double), 'a'))
        return -1;
      break;
    case 'S':
    case 's':
      if (!_osl_vformat_string(formatter, va_arg(argptr, const ichar*)))
        return -1;
      break;

    default:
      errno=ENOSYS;
      return -1;
    }
    psz++;
    start = psz;
  }
  return formatter->count;
}

//test test test test

static void _osl_make_format_string(ichar* format, const ichar* flags, int width, int precision, ichar fmt) {
  ichar* start = format;
  *format = '%';
  format++;
  if (flags) {
    size_t len = strlen(flags);
    memcpy(format, flags, len * sizeof(ichar));
    format += strlen(flags);
  }
  if (width >= 0) {
    format = _osl_vformat_utoa10(width, format);
  }
  if (precision >= 0) {
    *format = '.';
    format++;
    format = _osl_vformat_utoa10(precision, format);
  }
  *format = fmt;
  format++;
  *format = 0;
  assert((format - start) < 256);
}

int sys_vsnprintf(ichar* buffer, intptr_t count, const ichar* format, va_list ap) {
  int rv;
#ifdef _OSL_WINDOWS
  rv = _vsnprintf_s(buffer, count, count, format, ap);
  if (rv <= 0) {
    buffer[0] = 0;
  }
  else if (rv == count) {
    rv = (int)(count - 1);
    buffer[rv] = 0;
  }
#else // !_OSL_WINDOWS
  rv = vsnprintf(buffer, count, format, ap);
#endif // _OSL_WINDOWS
  return rv;
}

int _osl_cmp_snprintf(const ichar* format, ...) {
  ichar buffer1[512];
  ichar buffer2[512];
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

const ichar* fmt_flags = " 0#-+";
const ichar** all_fmt_flags = NULL;
int all_fmt_flag_count = 0;
const int max_width = 30;
const int max_precision = 40;

static int  osl_build_all_fmt_flags() {
  if (all_fmt_flags != NULL) {
    return all_fmt_flag_count;
  }
  int fmt_flags_len = 5;
  all_fmt_flag_count = 1 + 5 + 10 + 10 + 5 + 1;
  int buf_size = (all_fmt_flag_count + 1) * 6;
  ichar* buf = (ichar*)malloc(buf_size);
  ichar* pos = buf;
  const ichar** all_flags = (ichar**)malloc(sizeof(ichar*) * all_fmt_flag_count);
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

const ichar* int_formats = "oxXidu";
void _osl_printf_test_int(int value) {
  ichar format[256];
  int flags_count = osl_build_all_fmt_flags();
  const ichar* fpos = int_formats;
  while (*fpos) {
    for (int i = 0; i < flags_count; i++) {
      _osl_make_format_string(format, all_fmt_flags[i], -1, -1, *fpos);
      _osl_cmp_snprintf(format, value);
    }
    fpos++;
  }
  fpos = int_formats;
  while (*fpos) {
    for (int i = 0; i < flags_count; i++) {
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

const ichar* double_formats = "aefg";
void _osl_printf_test_double(double value) {
  ichar format[256];
  int flags_count = osl_build_all_fmt_flags();
  const ichar* fpos = double_formats; 
  while (*fpos) {
    for (int i = 0; i < flags_count; i++) {
      _osl_make_format_string(format, all_fmt_flags[i], -1, -1, *fpos);
      _osl_cmp_snprintf(format, value);
    }
    fpos++;
  }
  fpos = double_formats;
  while (*fpos) {
    for (int i = 0; i < flags_count; i++) {
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
 
//static const union { unsigned long __i[1]; float __d; } __Nanf = {0x7FC00000};
void osl_format_test_impl() { 
#ifdef _DEBUG
#define RUN_COUNT 0
#else 
#define RUN_COUNT 100000
#endif

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
 
#if RUN_COUNT != 0
  ichar buffer[256];
  int64_t t1 = osl_get_timer();
  for (int j = 0; j < RUN_COUNT; j++) {
    for (size_t i = 0; i < sizeof(float_val) / sizeof(float_val[0]); i++) {
      osl_snformat(buffer, 256, "%.17e", float_val[i]);
      osl_snformat(buffer, 256, "%.17f", float_val[i]);
      osl_snformat(buffer, 256, "%.17g", float_val[i]);
    } 
  }
  t1 = osl_get_timer() - t1;

  int64_t t2 = osl_get_timer();
  for (int j = 0; j < RUN_COUNT; j++) {
    for (size_t i = 0; i < sizeof(float_val) / sizeof(float_val[0]); i++) {
      snprintf(buffer, 256, "%.17e", float_val[i]);
      snprintf(buffer, 256, "%.17f", float_val[i]);
      snprintf(buffer, 256, "%.17g", float_val[i]);
    } 
  }
  t2 = osl_get_timer() - t2; 
  osl_printf("my:%ld,sys:%ld\n", t1, t2);
#endif //RUN_COUNT != 0 


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
  printf("\nprint nothing is ok.\n");
}


