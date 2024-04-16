 
#include <string.h> 
#include <assert.h>   
#include "format.h"
#ifndef FALSE
#define FALSE 0
#endif // FALSE

#ifndef TRUE
#define TRUE 1
#endif // TRUE

typedef int ibool;

int ieee754d64tos(double val, char* buf, int buflen, char specifier, int precision, int* pexp, int* pdotpos, char* pgret);


#define BIGNUM_ROUND_AWAY_FROM_ZERO 0
#define BIGNUM_IEEE754D64_USE_CACHE 1
 
typedef unsigned int BigNumDigit;
typedef int BigNumDigitDiff;
typedef uint64_t BigNumMulResult;
  
#define BIGNUM_BASE_POW 9 
#define BIGNUM_BASE 1000000000  
#define BIGNUM_SIZE 127

typedef struct BigNum BigNum;
struct BigNum { 
  int length;
  BigNumDigit digits[BIGNUM_SIZE];
};
  
//can contain uint64_t
#define BIGNUM_SMALL_SIZE 3 
typedef struct BigNumSmall BigNumSmall;
struct BigNumSmall {
  int length;
  BigNumDigit digits[BIGNUM_SMALL_SIZE];
};
 
#define DECINT_BASE 10
#define DECINT_BASE_SIZE 796
typedef struct DecInt DecInt;
struct DecInt {
  int length;
  char digits[DECINT_BASE_SIZE];
};
 
/// <summary>
/// init number to zero
/// </summary>
/// <param name="num"></param>
static void bignum_zero(BigNum* num) { 
  num->length = 1;
  num->digits[0] = 0;
}
 
static void bignum_uint64(BigNum* num, uint64_t val) {
  BigNumDigit* pos = num->digits;
  do {
    *pos = val % BIGNUM_BASE;
    pos++;
    val /= BIGNUM_BASE;
  } while (val);
  num->length = (int)(pos - num->digits);
  assert(num->length<= BIGNUM_SMALL_SIZE);
}
  
static void bignum_uint32(BigNum* num, uint32_t val) {
  BigNumDigit* pos = num->digits;
  do {
    *pos = val % BIGNUM_BASE;
    pos++;
    val /= BIGNUM_BASE;
  } while (val);
  num->length = (int)(pos - num->digits);
}
  
static void bignum_assign(BigNum* num, const BigNum* other) {
  if (num == other)
    return; 
  num->length = other->length;
  memcpy(num->digits, other->digits, sizeof(BigNumDigit) * num->length); 
}

static ibool bignum_is_zero(const BigNum* num) {
  if (num->length == 1) {
    return num->digits[0] == 0;
  }
  return FALSE;
}

static ibool bignum_is_base(const BigNum* num) {
  if (num->length == 2) {
    return  num->digits[0] == 0
      && num->digits[1] == 1;
  }
  return FALSE;
}

static uint32_t bignum_get_uint32(const BigNum* num) {
  uint32_t val = 0;
  for (const BigNumDigit* pos = num->digits + num->length - 1; pos >= num->digits; pos--) {
    val *= BIGNUM_BASE;
    val += *pos;
  } 
  return val;
}

static uint64_t bignum_get_uint64(const BigNum* num) {
  uint64_t val = 0;
  for (const BigNumDigit* pos = num->digits + num->length - 1; pos >= num->digits; pos--) {
    val *= BIGNUM_BASE;
    val += *pos;
  } 
  return val;
}

static int bignum_cmp(const BigNum* num, const BigNum* other) {
  if (num == other)
    return 0;
  if (num->length < other->length) {
    return -1;
  }
  else if (num->length > other->length) {
    return 1;
  }
  int len = num->length;
  const BigNumDigit* apos = num->digits;
  const BigNumDigit* bpos = other->digits;
  for (int i = len - 1; i >= 0; i--) {
    if (apos[i] < bpos[i]) {
      return -1;
    }
    else if (apos[i] > bpos[i]) {
      return 1;
    }
  }
  return 0;
}
 
static int bignum_add_impl(BigNum* c, const BigNum* b) {
  assert(c->length >= b->length);
  int len = b->length;
  const BigNumDigit* bpos = b->digits;

  BigNumDigit* cpos = c->digits;
  BigNumDigit carry = 0;
  int i;
  for (i = 0; i < len; i++) {
    carry += cpos[i] + bpos[i];
    cpos[i] = carry % BIGNUM_BASE;
    carry /= BIGNUM_BASE;
  }

  for (; (i < c->length) && carry; i++) {
    carry += cpos[i];
    cpos[i] = carry % BIGNUM_BASE;
    carry /= BIGNUM_BASE;
  }
  if (carry) {
    if (c->length < BIGNUM_SIZE) {
      cpos[c->length] = carry;
      c->length++;
    }
    else {
      assert(FALSE);
      return -1;
    }
  }
  return 0;
}

static int bignum_sub_impl(BigNum* c, const BigNum* b) {
  assert(c->length >= b->length);
  const BigNumDigit* bpos = b->digits;
  int len = b->length;
  int borrow = 0;
  BigNumDigit* clast = c->digits + c->length - 1;
  BigNumDigit* cpos = c->digits;
  int i;
  for (i = 0; i < len; i++) {
    BigNumDigitDiff r = cpos[i] - bpos[i] - borrow;
    if (r < 0) {
      borrow = 1;
      r += BIGNUM_BASE;
    }
    else {
      borrow = 0;
    }
    cpos[i] = r;
  }

  for (; (i < c->length) && borrow; i++) {
    BigNumDigitDiff r = cpos[i] - borrow;
    if (r < 0) {
      borrow = 1;
      r += BIGNUM_BASE;
    }
    else {
      borrow = 0;
    }
    cpos[i] = r;
  }
  if (borrow) {
    assert(FALSE);
    return -1;
  }
  for (int i = c->length - 1; i > 0 && (cpos[i] == 0); i--) {
    c->length--;
  }
  return 0;
}

static int bignum_add(BigNum* c, const BigNum* a, const BigNum* b) {
  BigNum tmp;
  if (a->length >= b->length) {
    if (c == b) {
      bignum_assign(&tmp, b);
      b = &tmp;
    }
    bignum_assign(c, a);
    return bignum_add_impl(c, b);
  }
  else {
    if (c == a) {
      bignum_assign(&tmp, a);
      a = &tmp;
    }
    bignum_assign(c, b);
    return bignum_add_impl(c, a);
  }
  return 0;
}

static int bignum_sub(BigNum* c, const BigNum* a, const BigNum* b) {
  BigNum tmp; 
  assert(bignum_cmp(a, b)>=0);
  if (c == b) {
    bignum_assign(&tmp, b);
    b = &tmp;
  }
  bignum_assign(c, a);
  return bignum_sub_impl(c, b); 
}

static int bignum_lshift(BigNum* c, int n) {
  assert(n >= 0);
  if (n == 0)
    return 0;
  if (c->length == 1 && c->digits[0] == 0)
    return 0;
  if ((c->length + n) > BIGNUM_SIZE) {
    assert(FALSE);
    return -1;
  }
  memmove(c->digits + n, c->digits, c->length * sizeof(BigNumDigit));
  memset(c->digits, 0, n * sizeof(BigNumDigit));
  c->length += n;
  return 0;
}

static int bignum_rshift(BigNum* c, int n) {
  assert(n >= 0);
  if (n == 0)
    return 0;
  if (c->length == 1) {
    *c->digits = 0;
    return 0;
  }
  int new_len = c->length - n;
  if (new_len <= 0) {
    bignum_zero(c);
    return 0;
  }
  memmove(c->digits, c->digits + n, new_len * sizeof(BigNumDigit));
  c->length -= n;
  return 0;
}

static int bignum_mul(BigNum* c, const BigNum* a, const BigNum* b) {
  if (bignum_is_base(b)) {
    if (c != a)
      bignum_assign(c, a);
    int rv = bignum_lshift(c, 1);
    if (rv != 0)
      return rv;
  }
  else {
    int len = a->length + b->length;
    if (len > BIGNUM_SIZE) {
      assert(FALSE);
      return -1;
    }
    BigNum tmpa;
    BigNum tmpb;
    if (c == a) {
      bignum_assign(&tmpa, a);
      a = &tmpa;
    }
    if (c == b) {
      bignum_assign(&tmpb, b);
      b = &tmpb;
    } 
    BigNumDigit* digits = c->digits;
    memset(digits, 0, sizeof(BigNumDigit) * len);
    c->length = len;
    const BigNumDigit* digits_a = a->digits;
    const BigNumDigit* digits_b = b->digits;
    int len_a = a->length;
    int len_b = b->length;
    for (int i = 0; i < len_a; i++) {
      BigNumMulResult up = 0;
      for (int j = 0; j < len_b; j++) {
        up += ((BigNumMulResult)digits[i + j]) 
          + ((BigNumMulResult)digits_a[i]) * ((BigNumMulResult)digits_b[j]);
        digits[i + j] = up % BIGNUM_BASE;
        up /= BIGNUM_BASE; 
      }
      assert(digits[i + len_b] == 0);
      digits[i + len_b] = (BigNumDigit)up;
    }

    for (int i = c->length - 1; i > 0 && digits[i] == 0; i--) {
      c->length--;
    }
  } 
  return 0;
}

static int bignum_mul_small(BigNum* c, const BigNum* a, uint32_t b) {
  assert(b<= BIGNUM_BASE);
  if (b == 0) {
    if (c != a)
      bignum_zero(c);
  }
  else if (b == 1) {
    if (c != a)
      bignum_assign(c, a);
  }
  else if (b == BIGNUM_BASE) {
    if (c != a)
      bignum_assign(c, a);
    int rv = bignum_lshift(c, 1);
    if (rv != 0)
      return rv;
  }
  else {
    int len = a->length+1;
    if (len > BIGNUM_SIZE) {
      assert(FALSE);
      return -1;
    }
    BigNum tmpa; 
    if (c == a) {
      bignum_assign(&tmpa, a);
      a = &tmpa;
    }   
    BigNumDigit* digits = c->digits;
    memset(digits, 0, sizeof(BigNumDigit) * len);
    c->length = len;
    const BigNumDigit* digits_a = a->digits; 
    int len_a = a->length; 
    for (int i = 0; i < len_a; i++) {
      BigNumMulResult up = 0;
      up += ((BigNumMulResult)digits[i])
        + ((BigNumMulResult)digits_a[i]) * ((BigNumMulResult)b);
      digits[i] = up % BIGNUM_BASE;
      up /= BIGNUM_BASE; 
      assert(digits[i + 1] == 0);
      digits[i + 1] = (BigNumDigit)up;
    }

    for (int i = c->length - 1; i > 0 && digits[i] == 0; i--) {
      c->length--;
    }
  }
  return 0;
}

static int bignum_div_rem(BigNum* c, BigNum* r, const BigNum* a, const BigNum* b) {
  assert(c != r);
  int cmp = bignum_cmp(a, b);
  if (cmp == 0) {
    if (c) {
      bignum_zero(c);
      c->digits[0] = 1;
    }
    if (r) {
      bignum_zero(r);
    }
    return 0;
  }
  else if (cmp < 0) {
    if (r) {
      bignum_assign(r, a);
    }
    if (c) {
      bignum_zero(c);
    }
    return 0;
  }

  if (b->length == 1 && b->digits[0] == 0) {
    return -1;
  }

  if (bignum_is_base(b)) {
    if (r) {
      r->length = 1;
      r->digits[0] = a->digits[0]; 
    }
    if (c) {
      if (c != a)
        bignum_assign(c, a);
      bignum_rshift(c, 1);
    }
  }
  else {
    BigNum rem;
    BigNum divider;
    bignum_assign(&rem, a);
    bignum_assign(&divider, b);
    if (c)
      bignum_zero(c);
    int divn = (int)(rem.length - divider.length);
    bignum_lshift(&divider, divn); 
    int idx = rem.length - 1;
    for (int i = 0; i <= divn; i++, idx--) {
      if (bignum_cmp(&rem, &divider) < 0) {
        bignum_rshift(&divider, 1);
        continue;
      }
      BigNumDigit rem_val;
      BigNumDigit div_val;
      rem_val = rem.digits[idx];
      if (rem.length > divider.length) {
        rem_val = rem_val + rem.digits[idx + 1] * BIGNUM_BASE;
      }
      div_val = divider.digits[idx];
      BigNumDigit r = (rem_val + 1) / div_val;
      do { 
        BigNum guess; 
        bignum_mul_small(&guess, &divider,r);
        while (bignum_cmp(&guess, &rem) > 0) {
          bignum_sub_impl(&guess, &divider);
          r--;
        }
        bignum_sub_impl(&rem, &guess);
      } while (0);
      if (r != 0) {
        if (c) {
          BigNum result;
          bignum_uint32(&result, r);
          bignum_lshift(&result, divn - i);
          bignum_add(c, c, &result);
        }
      }
      if (rem.length == 1 && rem.digits[0] == 0)
        break;
      bignum_rshift(&divider, 1);
    }
    if (r) {
      bignum_assign(r, &rem);
    }
  } 
  return 0;
}

static int bignum_div(BigNum* c, const BigNum* a, const BigNum* b) {
  return bignum_div_rem(c, NULL, a, b);
}

static int bignum_rem(BigNum* c, const BigNum* a, const BigNum* b) {
  return bignum_div_rem(NULL, c, a, b);
}

static int bignum_pow32(BigNum* c, const BigNum* a, uint32_t b) {
  BigNum tmp;
  BigNum t;
  if (c == a) {
    bignum_assign(&tmp, a);
    a = &tmp;
  }
  if (b == 0) {
    c->length = 1;
    c->digits[0] = 1; 
    return 0;
  }
  bignum_assign(c, a);
  if (b == 1)
    return 0;
  bignum_assign(&t, a);
  for (uint32_t i = b - 1; i; i >>= 1) {
    if (i & 1) {
      bignum_mul(c, c, &t);
    }
    if (i >> 1)
      bignum_mul(&t, &t, &t);
  }
  return 0;
}

#define digit_to_dec(buf,val,n)\
do{\
  assert(n <= DECINT_BASE_SIZE);\
  (buf)[n] = (val) % 10; \
  (val) /= 10;\
  (n)++;\
}while(0)
 
#define try_digit_to_dec(buf,val,n)\
do{\
  if(val)\
    digit_to_dec(buf,val,n); \
}while(0)

static int bignum_to_dec(const BigNum* num, char* buf, int* ppow10) {
  int n = 0;
  BigNumDigit val;
  int lasrIdx = num->length - 1;
  
  const BigNumDigit* digits = num->digits;
  int i;
  for (  i = 0; i < lasrIdx; i++) {
    if (digits[i] == 0) 
      (*ppow10) -= BIGNUM_BASE_POW; 
    else
      break;
  }
  assert(((lasrIdx-i) * BIGNUM_BASE_POW) <= DECINT_BASE_SIZE);
  for ( ; i < lasrIdx; i++) {
    val = digits[i]; 
    digit_to_dec(buf, val, n);
    digit_to_dec(buf,val,n); digit_to_dec(buf, val, n); 
    digit_to_dec(buf, val, n); digit_to_dec(buf, val, n);
    digit_to_dec(buf, val, n); digit_to_dec(buf, val, n); 
    digit_to_dec(buf, val, n); digit_to_dec(buf, val, n); 
  }
  val = digits[lasrIdx];
  digit_to_dec(buf, val, n);
  try_digit_to_dec(buf,val,n); try_digit_to_dec(buf, val, n);
  try_digit_to_dec(buf, val, n); try_digit_to_dec(buf, val, n);
  try_digit_to_dec(buf, val, n); try_digit_to_dec(buf, val, n);
  try_digit_to_dec(buf, val, n); try_digit_to_dec(buf, val, n);
  
  assert(n <= DECINT_BASE_SIZE);
  buf[n] = 0;
  return n;
}

 

#if BIGNUM_IEEE754D64_USE_CACHE ==1
static const BigNum* bignum_5_pow_for_ieee754d64(int n) {
  // orign_exp: 0_2047
  // exp=orign_exp-1023: -1023_1024
  // exp-52:-1075_972
  // 52-exp:1075_-972
  assert(n>=0 && n< 1076);
  static BigNum nums[1076]; 
  BigNum* ret = nums + n;
  if (ret->length == 0) {
    static const BigNumSmall big_5 = { 1,{5} };
    if (0 != bignum_pow32(ret, (const BigNum*)&big_5,n)) {
      assert(FALSE);
      return NULL;
    }
  }
  return ret;
}

static const BigNum* bignum_2_pow_for_ieee754d64(int n) {
  //orign_exp: 0_2047
  //exp=orign_exp-1023: -1023_1024
  // exp-52:-1075_972
  // 52-exp:1075_-972
  assert(n >= 0 && n < 1076);
  static BigNum nums[1076];
  BigNum* ret = nums + n;
  if (ret->length == 0) {
    static const BigNumSmall big_2 = { 1,{2} };
    if (0 != bignum_pow32(ret, (const BigNum*)&big_2, n)) {
      assert(FALSE);
      return NULL;
    }
  }
  return ret;
}
 
#endif // BIGNUM_IEEE754D64_USE_CACHE

typedef double float64_t;
union ui64_f64 { uint64_t ui; float64_t f; };
//#define signF64UI( a ) ((int_fast16_t) ((uint64_t) (a)>>63))
#define expF64UI( a ) ((int32_t) ((a)>>52) & 0x7FF)
#define fracF64UI( a ) ((a) & UINT64_C( 0x000FFFFFFFFFFFFF ))
//#define packToF64UI( sign, exp, sig ) ((uint64_t) (((uint_fast64_t) (sign)<<63) + ((uint_fast64_t) (exp)<<52) + (sig)))
 
static int bignum_ieee754d64(BigNum* num, double value, int* pow10) {
  union ui64_f64 ua;
  ua.f = value;
  uint64_t ui = ua.ui;
  //uint64_t sign = signF64UI(ui);
  int32_t exp = expF64UI(ui);
  uint64_t frac = fracF64UI(ui);
  if (frac == 0 && exp==0) {
    exp = 0;
    frac = 0;
    bignum_zero(num);
    *pow10 = 0;
    return 0;
  }
  // nan or inf exp=2047
  // normal 0<exp<2047      (1+0.frac)*pow(2,exp)
  // subnormal exp == 0     0.frac*pow(2,-1022)
  exp -= 1023;
  if (exp >= 52) {
    *pow10 = 0; 
     
#if BIGNUM_IEEE754D64_USE_CACHE == 0
    static const BigNumSmall big_2 = { 1,{2} };
    //pow(2, exp);
    BigNum big_2_pow_exp;
    if (0 != bignum_pow32(&big_2_pow_exp, (const BigNum*)&big_2, exp)) {
      assert(FALSE);
      return -1;
    }
     
    //pow(2, exp-52);
    BigNum big_2_pow_exp_sub_52;
    if (0 != bignum_pow32(&big_2_pow_exp_sub_52, (const BigNum*)&big_2, exp - 52)) {
      assert(FALSE);
      return -1;
    }

    BigNumSmall big_frac;
    bignum_uint64((BigNum*)&big_frac, frac);
 
    //frac * pow(2, exp-52);
    BigNum big_frac_mul_2_pow_exp_sub_52;
    if (0 != bignum_mul(&big_frac_mul_2_pow_exp_sub_52, (const BigNum*)&big_frac, &big_2_pow_exp_sub_52)) {
      assert(FALSE);
      return -1;
    }
    //pow(2, exp) + frac * pow(2, exp-52);
    return bignum_add(num, &big_2_pow_exp, &big_frac_mul_2_pow_exp_sub_52);
#else //BIGNUM_IEEE754D64_USE_CACHE==1
    //pow(2, exp);
    const BigNum* big_2_pow_exp = bignum_2_pow_for_ieee754d64(exp); 

    //pow(2, exp-52);
    const BigNum* big_2_pow_exp_sub_52 = bignum_2_pow_for_ieee754d64(exp-52); 
    BigNumSmall big_frac;
    bignum_uint64((BigNum*)&big_frac, frac);

    //frac * pow(2, exp-52);
    BigNum big_frac_mul_2_pow_exp_sub_52;
    if (0 != bignum_mul(&big_frac_mul_2_pow_exp_sub_52, (const BigNum*)&big_frac, big_2_pow_exp_sub_52)) {
      assert(FALSE);
      return -1;
    }    
    //pow(2, exp) + frac * pow(2, exp-52);
    return bignum_add(num, big_2_pow_exp, &big_frac_mul_2_pow_exp_sub_52);
#endif // BIGNUM_IEEE754D64_USE_CACHE 
  }
  else { 
    static const BigNumSmall big_5 = { 1,{5} };
    //(pow(2, 52)+frac)
    //subnormal
    if (exp == -1023) {// *pow(5,1074)
      *pow10 = 52 - exp-1;
      // y=frac * pow(2,-1022-52)
      // y*pow(10,1074) = frac * pow(2, 1074)*pow(10,1074)
      // = frac * pow(2, exp-52)*pow(2,1074)*pow(5,1074)
      // = frac *pow(5,1074)
      BigNumSmall big_frac;
      bignum_uint64((BigNum*)&big_frac, frac );
 
      //pow(5, 1074)
#if  BIGNUM_IEEE754D64_USE_CACHE == 0
      BigNum big_5_pow_1074;//todo:cache?
      if (0 != bignum_pow32(&big_5_pow_1074, (const BigNum*)&big_5, 1074)) {
        assert(FALSE);
        return -1;
      }
      // frac *pow(5,1074)
      return bignum_mul(num, (const BigNum*)&big_frac, &big_5_pow_1074);
#else // BIGNUM_IEEE754D64_USE_CACHE==1
      const BigNum* big_5_pow_1074 = bignum_5_pow_for_ieee754d64(1074);  
      // frac *pow(5,1074)
      return bignum_mul(num, (const BigNum*)&big_frac, big_5_pow_1074);
#endif // BIGNUM_IEEE754D64_USE_CACHE

    }
    else {//*pow(10,52-exp)
      *pow10 = 52 - exp;
      // y=pow(2, exp) + frac * pow(2, exp-52);
      // y*pow(10,52-exp) = pow(2, exp)*pow(10,52-exp)  + frac * pow(2, exp-52)*pow(10,52-exp);
      // =  pow(2, exp)*pow(2,52-exp)*pow(5,52-exp)  + frac * pow(2, exp-52)*pow(2,52-exp)*pow(5,52-exp);
      // = pow(2, 52)*pow(5,52-exp)+frac*pow(5,52-exp)
      // = (pow(2, 52)+frac)*pow(5,52-exp)
      BigNumSmall big_2_pow_52_add_frac;
      bignum_uint64((BigNum*)&big_2_pow_52_add_frac, frac + (UINT64_C(1) << 52));
#if  BIGNUM_IEEE754D64_USE_CACHE == 0
      //pow(5, 52 - exp)
      BigNum big_5_pow_52_sub_exp;//todo:cache?
      if (0 != bignum_pow32(&big_5_pow_52_sub_exp, (const BigNum*)&big_5, 52 - exp)) {
        assert(FALSE);
        return -1;
      }
      //(pow(2, 52) + frac) * pow(5, 52 - exp)
      return bignum_mul(num, (const BigNum*)&big_2_pow_52_add_frac, &big_5_pow_52_sub_exp);
#else // BIGNUM_IEEE754D64_USE_CACHE == 1
      //pow(5, 52 - exp)
      const BigNum* big_5_pow_52_sub_exp = bignum_5_pow_for_ieee754d64(52 - exp);
      //(pow(2, 52) + frac) * pow(5, 52 - exp)
      return bignum_mul(num, (const BigNum*)&big_2_pow_52_add_frac, big_5_pow_52_sub_exp);
#endif // BIGNUM_IEEE754D64_USE_CACHE
    } 
  }
}

static int decint_ieee754d64(char* buf, double value, int* ppow10) {
  BigNum num;
  int rv = bignum_ieee754d64(&num, value, ppow10);
  if (rv != 0)
    return rv;
  return bignum_to_dec(&num, buf, ppow10);
}
 
static void decint_assign(DecInt* num, const DecInt* other) {
  if (num == other)
    return; 
  num->length = other->length;
  memcpy(num->digits, other->digits, sizeof(char) * num->length); 
}
 
static int decint_add_small(DecInt* c, int b) {
  assert(0<=b&&b<=9); 
  char* cpos = c->digits;
  int carry = 0;
  
  carry += cpos[0] + b;
  cpos[0] = carry % DECINT_BASE;
  carry /= DECINT_BASE; 

  int len = c->length;
  for (int i=1; (i < len) && carry; i++) {
    carry += cpos[i];
    cpos[i] = carry % DECINT_BASE;
    carry /= DECINT_BASE;
  }
  if (carry) {
    if (c->length < DECINT_BASE_SIZE) {
      cpos[c->length] = carry;
      c->length++;
    }
    else {
      assert(FALSE);
      return -1;
    }
  }
  return 0;
}
 
static int decint_lshift(DecInt* c, int n) {
  assert(n >= 0);
  if (n == 0)
    return 0;
  if (c->length == 1 && c->digits[0] == 0)
    return 0;
  if ((c->length + n) > DECINT_BASE_SIZE) {
    assert(FALSE);
    return -1;
  }
  memmove(c->digits + n, c->digits, c->length * sizeof(char));
  memset(c->digits, 0, n * sizeof(char));
  c->length += n;
  return 0;
}

static int decint_rshift(DecInt* c, int n) {
  assert(n >= 0);
  if (n == 0)
    return 0;
  if (c->length == 1) {
    c->digits[0] = 0;
    return 0;
  }
  int new_len = c->length - n;
  if (new_len <= 0) {
    c->length = 1;
    c->digits[0] = 0;
    return 0;
  }
  memmove(c->digits, c->digits + n, new_len * sizeof(char));
  c->length -= n;
  return 0;
}
 
static int decint_to_string(const DecInt* num, char* buf, int buflen) {
  int len = buflen;
  if (len > num->length)
    len = num->length;
  for (int i = 0; i < len; i++) {
    buf[i] = '0' + num->digits[num->length - 1 - i];
  }
  buf[len] = 0;
  return (int)len;
}
 
#if  BIGNUM_ROUND_AWAY_FROM_ZERO == 1

static int decint_convert_round_to(DecInt* result, const DecInt* num, int round_pos) {
  int carry = 0;
  int shift = num->length - round_pos;

  if (shift >= 0) {  
    decint_rshift(result, shift);
    int  old_len = result->length;
    decint_add_small(result, 5);
    if (result->length > old_len) {
      carry = 1;
    }
    decint_rshift(result, 1); 
  }
  else {
    //no more precision
  }
  return carry;
}

#else //near even

static int decint_convert_round_to(DecInt* result, const DecInt* num, int round_pos) {
  int carry = 0;
  int shift = num->length - round_pos;

  if (shift >= 0) {
    char round_bit = num->digits[shift];
    if (round_bit == 5) {
      //check sticky_bits;
      ibool sticky_bits_is_zero = TRUE;
      for (int i = 0; i < shift; i++) {
        if (num->digits[i] != 0) {
          sticky_bits_is_zero = FALSE;
          break;
        }
      }
      if (sticky_bits_is_zero) {
        char guard_bit = num->digits[shift + 1];
        if ((guard_bit % 2) == 0) {
          decint_assign(result, num);
          decint_rshift(result, shift + 1);
          return 0;
        }
      }
    }
      
    decint_rshift(result, shift);
    int old_len = result->length;
    decint_add_small(result, 5);
    if (result->length > old_len) {
      carry = 1;
    }
    decint_rshift(result, 1);
  }
  else {
    //no more precision
  }
  return carry;
}

#endif

 
int ieee754d64tos(double value, char* buf, int buflen, char specifier,
  int precision, int* pexp, int* pdotpos, char* gspecifier) {
  *pexp = 0;
  *pdotpos = 0;

  DecInt num;
  DecInt result = { -1 };
  int pow10;
#if 0 // cache last value
  static int  cached = FALSE;
  static double cache_value = 0;
  static int cache_pow10;
  static DecInt cache;
  if (cached && cache_value == value) {
    pow10 = cache_pow10;
    decint_assign(&num, &cache);
  }
  else {
    num.length = decint_ieee754d64(num.digits, value, &pow10);
    if (num.length < 0) {
      buf[0] = 0;
      return -1;
    }
    cached = TRUE;
    decint_assign(&cache, &num);
    cache_value = value;
    cache_pow10 = pow10;
  }
#else
  num.length = decint_ieee754d64(num.digits, value, &pow10);
  if (num.length < 0) {
    buf[0] = 0;
    return -1;
  }
#endif  
    
  char orign_specifier = specifier;
  int exp = num.length - pow10 - 1;
  *pexp = exp;

  if (precision < 0)
    precision = 6;
  int orign_precision = precision;

  if (specifier == 'g') {
    if (precision == 0) {
      precision = 1;
      orign_precision = 1;
    }
    if (exp < -4 || (exp >= precision)) {
      specifier = 'e';
      precision--;
    }
    else {
      specifier = 'f';
      precision -= -pow10 + num.length;
    }
    if (gspecifier)
      *gspecifier = specifier;
  }
again:
  decint_assign(&result, &num);
  if (specifier == 'f') {
    int dotpos = -pow10 + num.length;
    *pdotpos = dotpos;
    if (pow10 != 0) {
      int round_pos = dotpos + precision + 1;
      if (round_pos > 0) {
        if (decint_convert_round_to(&result, &num, round_pos)) {
          if (orign_specifier == 'g') {
            int new_exp = exp + 1;
            if (new_exp < -4 || new_exp >= orign_precision) {
              precision = orign_precision - 1;
              specifier = orign_specifier;
              orign_specifier = 0;
              specifier = 'e';
              if (gspecifier)
                *gspecifier = 'e';
              goto again;
            }
            else {
              (*pdotpos)++;
            }
          }
          else {
            (*pdotpos)++;
          }
        }
      }
      else {
        //none of the digits
        *pdotpos = 0;
        buf[0] = 0;
        return 0;
      }
    }
    else {
      //full of the digits
    }
  }
  else if (specifier == 'e') {
    *pdotpos = 1;
    int round_pos = 1 + precision + 1;
    if (decint_convert_round_to(&result, &num, round_pos)) {
      if (orign_specifier == 'g') {
        int new_exp = exp + 1;
        if (new_exp >= -4 && new_exp < orign_precision) {
          precision = orign_precision - (-pow10 + num.length);
          specifier = orign_specifier;
          orign_specifier = 0;
          specifier = 'f';
          if (gspecifier)
            *gspecifier = 'f';
          goto again;
        }
        else {
          (*pexp)++;
        }
      }
      else {
        (*pexp)++;
      }
    }
  }
  else {
    assert(FALSE);
  }
  decint_to_string(&result, buf, buflen);
  return 0;
}
 


 



