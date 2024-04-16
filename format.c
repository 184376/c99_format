 
#include <string.h>   
#include <errno.h>   
#include <assert.h>  
 
#include "format.h"

#ifndef FALSE
#define FALSE 0
#endif // FALSE

#ifndef TRUE
#define TRUE 1
#endif // TRUE


#if defined(_WIN32) || defined(_WIN64)
#define _OS_WINDOWS 
#endif
  
#define NUMBER_BUFFER_LENGTH 511
#define NUMBER_DEFAULT_PRECISION 6
typedef int ibool;

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
  void* userData;
  intptr_t count;

  char tempbuf[NUMBER_BUFFER_LENGTH + 1];
  //char cvtbuf[NUMBER_BUFFER_LENGTH + 1];
};

static intptr_t _vformat_null_write(void* arg, const char* sz, intptr_t len) {
  return len;
}
 
static intptr_t _vformat_impl(OslFormatter* formatter, const char* szformat, va_list argptr);

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
intptr_t osl_vformat(OslFormatWriteFunc writefunc, void* userData, const char* szformat, va_list argptr) {
  OslFormatter formatter;
  formatter.count = 0;
  formatter.writefunc = writefunc;
  formatter.userData = userData;
  if (writefunc == NULL)
    formatter.writefunc = _vformat_null_write;
  return _vformat_impl(&formatter, szformat, argptr);
}   
static const char _digits_upper[] = "0123456789ABCDEF";
static const char _digits_lower[] = "0123456789abcdef";
#define INT_STR_BUF_LENGTH 64 
#define DEFINE_ITOA(name,type,base)\
static char* name(type value, char* buf) {\
  char tmp[INT_STR_BUF_LENGTH];\
  int n = 0;\
  do {\
    tmp[n] = '0'+(value % base);\
    value /= base;\
    n++;\
  } while (value != 0);\
  n--;\
  assert(n < INT_STR_BUF_LENGTH);\
  char* pos = buf;\
  for (int i = n; i >= 0; i--) {\
    *pos = tmp[i];\
    pos++;\
  }\
  *pos = 0;\
  return pos;\
}

#define DEFINE_ITOA16(name,type)\
static char* name(type value, char* buf, ibool is_upper) {\
  const char* digits;\
  char tmp[INT_STR_BUF_LENGTH];\
  digits = is_upper ? _digits_upper : _digits_lower;\
  int n = 0;\
  do {\
    tmp[n] = digits[value % 16];\
    value /= 16;\
    n++;\
  } while (value != 0);\
  n--;\
  assert(n < INT_STR_BUF_LENGTH);\
  char* pos = buf;\
  for (int i = n; i >= 0; i--) {\
    *pos = tmp[i];\
    pos++;\
  }\
  *pos = 0;\
  return pos;\
}
 
DEFINE_ITOA(_vformat_u32toa10, uint32_t, 10)
DEFINE_ITOA(_vformat_u64toa10, uint64_t, 10)
DEFINE_ITOA16(_vformat_u64toa16, uint64_t)

static ibool _vformat_append(OslFormatter* formatter, const char* start, intptr_t len) {
  if (len <= 0)
    return TRUE;
  if (formatter->writefunc(formatter->userData, start, len) < 0)
    return FALSE;
  formatter->count += len;
  return TRUE;
}

static ibool _vformat_append_nchar(OslFormatter* formatter, char ch, intptr_t count) {
  if (count <= 0)
    return TRUE;
  for (intptr_t i = 0; i < count; i++) {
    if (formatter->writefunc(formatter->userData, &ch, 1) < 0)
      return FALSE;
  }
  formatter->count += count;
  return TRUE;
}
 
static ibool _vformat_append_with_prefix(OslFormatter* formatter, const char* prefix, int prefixLen, const char* digits, intptr_t len) {

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
    if (!_vformat_append_nchar(formatter, ' ', padding_black_count))
      return FALSE;
    if (prefix && !_vformat_append(formatter, prefix, prefixLen)) {
      return FALSE;
    }

    if (!_vformat_append_nchar(formatter, '0', padding_zero_count))
      return FALSE;
  }
  else {
    if (prefix && !_vformat_append(formatter, prefix, prefixLen)) {
      return FALSE;
    }
    if (!_vformat_append_nchar(formatter, '0', padding_zero_count))
      return FALSE;
    padding_zero_count = 0;
  }

  if (!_vformat_append(formatter, digits, len))
    return FALSE;

  if (formatter->left_align) {
    if (!_vformat_append_nchar(formatter, ' ', padding_black_count + padding_zero_count))
      return FALSE;
  }

  return TRUE;
}

static ibool _vformat_append_with_prefix_double(OslFormatter* formatter, const char* prefix, int prefixLen, const char* digits, intptr_t len) {
  
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
    if (!_vformat_append_nchar(formatter, ' ', padding_black_count))
      return FALSE;
    if (prefix && !_vformat_append(formatter, prefix, prefixLen)) {
      return FALSE;
    }
    if (!_vformat_append_nchar(formatter, '0', padding_zero_count))
      return FALSE;
  }
  else if (prefix && !_vformat_append(formatter, prefix, prefixLen)) {
    return FALSE;
  }

  if (!_vformat_append(formatter, digits, len))
    return FALSE;

  if (formatter->left_align) {
    if (!_vformat_append_nchar(formatter, ' ', padding_black_count))
      return FALSE;
  }

  return TRUE;
}

static ibool _vformat_append_integer(OslFormatter* formatter, unsigned int base, ibool neg, const char* digits, intptr_t len) {

  const char* prefix = NULL;
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

  return _vformat_append_with_prefix(formatter, prefix, prefixLen, digits, len);
}
 
static ibool _vformat_uint64(OslFormatter* formatter, uint64_t value, unsigned int base, ibool neg) {
  const char* digits;
  char* buf = formatter->tempbuf;
  char* pos;
  intptr_t len;
  //If both the converted value and the precision are ?0? the conversion results in no characters.
  if (value == 0 && formatter->precision == 0)
    return _vformat_append_integer(formatter, base, neg, NULL, 0);
  if (formatter->specifieris_upper)
    digits = _digits_upper;
  else
    digits = _digits_lower;
  pos = buf + NUMBER_BUFFER_LENGTH;

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
  len = buf + NUMBER_BUFFER_LENGTH - pos + 1;
  return _vformat_append_integer(formatter, base, neg, pos, len);
}

static ibool _vformat_int64(OslFormatter* formatter, int64_t value, unsigned int base) {
  uint64_t val;
  ibool neg;
  if (value < 0) {
    val = -value; neg = TRUE;
  }
  else {
    val = value; neg = FALSE;
  }

  return _vformat_uint64(formatter, val, base, neg);
}

static ibool _vformat_append_string(OslFormatter* formatter, const char* sz, intptr_t len) {
  if (len < formatter->width) {
    if (!formatter->left_align) {
      intptr_t delta = formatter->width - len;
      while (delta) {
        delta--;
        if (!_vformat_append(formatter, " ", 1))
          return FALSE;
      }
    }
  }
  if (!_vformat_append(formatter, sz, len))
    return FALSE;
  if (len < formatter->width) {
    if (formatter->left_align) {
      intptr_t delta = formatter->width - len;
      while (delta) {
        delta--;
        if (!_vformat_append(formatter, " ", 1))
          return FALSE;
      }
    }
  }
  return TRUE;
}

static ibool _vformat_append_double(OslFormatter* formatter, ibool neg, const char* digits, intptr_t len) {
  const char* prefix = NULL;
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

  return _vformat_append_with_prefix_double(formatter, prefix, prefixLen, digits, len);
}

static ibool _vformat_string(OslFormatter* formatter, const char* sz) {
  intptr_t len;
  if (sz == NULL) {
    sz = "(null)";
  }
  len = strlen(sz);
  if (formatter->precision >= 0 && len > formatter->precision) {
    len = formatter->precision;
  }
  return _vformat_append_string(formatter, sz, len);
}

static char* _vformat_remove_trailing_zero_guard(const char* guard, const char* pos, ibool keepDot) {
  while (pos > guard && (*pos == '0')) {
    pos--;
  }
  if (!keepDot && *pos == '.') {
    return (char*)pos;
  }
  return (char*)(pos + 1);
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
 
static intptr_t _vformat_hcvt_ieee754d64(OslFormatter* formatter, char* buf,
  intptr_t  buf_count,
  double  value,
  int precision, int* prefix_len) {
    
  *prefix_len = 2; 
  char* pos = buf;
   
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
  char tmp[32];
  _vformat_u64toa16(mantissa, tmp,formatter->specifieris_upper);
  const char* tpos = tmp;

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
  pos = _vformat_u32toa10(exponent, pos);
  *pos = 0;
  return pos-buf;
}

static intptr_t _vformat_double_f(OslFormatter* formatter, char* buf, const char* cvt,
  int precision, ibool significant_precision, int dotpos, ibool trailingZeroes) {
  char* pos = buf;
  char* trailing_limit = buf;
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
    pos = _vformat_remove_trailing_zero_guard(trailing_limit, pos - 1, formatter->alternate_form);
  }

  return pos - buf;
}

static intptr_t _vformat_double_e(OslFormatter* formatter, char* buf, const char* cvt,
  int precision, ibool significant_precision, int exponent, int dotpos, ibool trailingZeroes) {
  intptr_t len = _vformat_double_f(formatter, buf, cvt, precision, significant_precision, dotpos, trailingZeroes);
  char* pos = buf + len;

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

  char* digits = pos + exponent_len - 1;

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
  char value[9];
};

static ibool _vformat_nan(OslFormatter* formatter, int negative,int classification,char specifier) {
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
    const char* digits = nan->value;
    int digist_len = nan->len;
    char* prefix = formatter->tempbuf;
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
    return _vformat_append_with_prefix_double(formatter, prefix, prefix_len, digits, digist_len); 
   
}

extern int ieee754d64tos(double value, char* buf, int buflen,   char specifier, int precision, int* pexp, int* pdotpos, char* pgret);

static ibool _vformat_ieee754d64(OslFormatter* formatter, double value, char specifier) {
   
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
    return _vformat_nan(formatter, negative, classification, specifier);
  }  

  int precision;
  intptr_t len = 0;
  int exponent10;
  int dotpos;
  char gspecifier;
  char cvtbuf[NUMBER_BUFFER_LENGTH + 1];

  switch (specifier) {
  case 'a':
    do{ 
      int prefix_len;
      precision = formatter->precision >= 0 ? formatter->precision : 13;
      len = _vformat_hcvt_ieee754d64(formatter, formatter->tempbuf,
        NUMBER_BUFFER_LENGTH, value, precision, &prefix_len);
      if (len <0)
        return FALSE; 
      if (len >= formatter->width || formatter->left_align)
        return _vformat_append_string(formatter, formatter->tempbuf, len);
      else {
        if (formatter->padding_zero) {
          if (!_vformat_append(formatter, formatter->tempbuf, prefix_len))
            return FALSE;
          if (!_vformat_append_nchar(formatter, '0', formatter->width - len))
            return FALSE;
        }
        else {
          if (!_vformat_append_nchar(formatter, ' ', formatter->width - len))
            return FALSE;
          if (!_vformat_append(formatter, formatter->tempbuf, prefix_len))
            return FALSE;
        }
        return _vformat_append(formatter, formatter->tempbuf + prefix_len, len - prefix_len);
      }
    }while (0);
  case 'e':
    precision = formatter->precision >= 0 ? formatter->precision : 6;
    ieee754d64tos(value, cvtbuf, NUMBER_BUFFER_LENGTH,   'e',
      precision, &exponent10, &dotpos, NULL);
     
    len = _vformat_double_e(
      formatter,
      formatter->tempbuf,
      cvtbuf,
      precision, FALSE, exponent10, dotpos, TRUE);
    break;
  case 'f':
    precision = (formatter->precision >= 0 ? formatter->precision : 6);
    ieee754d64tos(value, cvtbuf, NUMBER_BUFFER_LENGTH,  'f',
      precision, &exponent10, &dotpos, NULL);
    len = _vformat_double_f(
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
    ieee754d64tos(value, cvtbuf, NUMBER_BUFFER_LENGTH,'g',
      precision, &exponent10, &dotpos, &gspecifier);
    //'#' For g and G
    //  conversions, trailing zeros are not removed from the
    //  result as they would otherwise be.
    // 
    //Style e is used if the exponent from its conversion is less than -4 
    // or greater than or equal to the precision.
    if (gspecifier == 'e') {//if ((exp < -4 || (exp >= precision))) {
      len = _vformat_double_e(
        formatter,
        formatter->tempbuf,
        cvtbuf,
        precision, TRUE, exponent10, dotpos, formatter->alternate_form);
    }
    else {
      len = _vformat_double_f(
        formatter,
        formatter->tempbuf,
        cvtbuf,
        precision, TRUE, dotpos, formatter->alternate_form);
    }   
    break;
  default:
    return FALSE;
  } 
  return _vformat_append_double(formatter, value < 0, formatter->tempbuf, len);
}

static intptr_t _vformat_impl(OslFormatter* formatter, const char* szformat, va_list argptr) {
  const char* psz;
  const char* start;

  psz = szformat;
  start = psz;

  for (;;) {

    ibool dot_without_precision;
    while (*psz && *psz != '%') {
      psz++;
    }

    if (*psz == 0) {
      if (!_vformat_append(formatter, start, psz - start))
        return -1;
      break;
    }
    psz++;
    if (*psz == '%') {
      if (!_vformat_append(formatter, start, psz - start))
        return -1;
      psz++;
      start = psz;
      continue;
    }

    if ((psz - start - 1) > 0
      && !_vformat_append(formatter, start, psz - start - 1))
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
    char char_val;
    switch (*psz) {
    case 'c':
      char_val = va_arg(argptr, int);
      if (!_vformat_append(formatter, &char_val, 1))
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

      if (!_vformat_int64(formatter, i_val, 10))
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

      if (!_vformat_uint64(formatter, u_val, 10, FALSE))
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

      if (!_vformat_uint64(formatter, u_val, 8, FALSE))
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

      if (!_vformat_uint64(formatter, u_val, 16, FALSE))
        return -1;
      break;
    case 'p':
      //Pointer address
#ifdef _OS_WINDOWS
      formatter->specifieris_upper = TRUE;
#else // !_OS_WINDOWS
      formatter->extra_flag = TRUE;
      formatter->specifieris_upper = FALSE;
#endif // _OS_WINDOWS
      formatter->with_sign = FALSE;
      formatter->padding_zero = TRUE;
      if (sizeof(void*) == sizeof(uint64_t)) {
        formatter->width = 16;
        if (!_vformat_uint64(formatter, va_arg(argptr, uint64_t), 16, FALSE))
          return -1;
      }
      else {
        formatter->width = 8;
        if (!_vformat_uint64(formatter, va_arg(argptr, unsigned int), 16, FALSE))
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
      if (!_vformat_ieee754d64(formatter, va_arg(argptr, double), 'f'))
        return -1;
      break;
    case 'E':
      formatter->specifieris_upper = TRUE;
    case 'e':
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_vformat_ieee754d64(formatter, va_arg(argptr, double), 'e'))
        return -1;
      break;
    case 'G':
      formatter->specifieris_upper = TRUE;
    case 'g':
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_vformat_ieee754d64(formatter, va_arg(argptr, double), 'g'))
        return -1;
      break;
    case 'A'://Hexadecimal floating point, uppercase
      formatter->specifieris_upper = TRUE;
    case 'a'://Hexadecimal floating point, lowercase
      if (dot_without_precision)
        formatter->precision = 0;
      if (!_vformat_ieee754d64(formatter, va_arg(argptr, double), 'a'))
        return -1;
      break;
    case 'S':
    case 's':
      if (!_vformat_string(formatter, va_arg(argptr, const char*)))
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
 