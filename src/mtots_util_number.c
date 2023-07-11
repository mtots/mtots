#include "mtots_util_number.h"

#include <math.h>


ubool doubleIsI32(double value) {
  return value == (double)(i32)value;
}

double mfmod(double a, double b) {
  /* If fmod returns zero, we just return zero.
   *
   * fmod always returns the same sign as `a`, so
   * if `a` and `b` have the same sign, we can just
   * return what fmod returns.
   *
   * If the signs differ and fmod is non-zero,
   *   - and b > 0, we add b to make the result positive,
   *   - and b < 0, we add b to make the result negative */
  double f = fmod(a, b);
  if (f == 0) {
    return 0; /* Even if f is negative zero, we return positive zero */
  }
  if ((a < 0) == (b < 0)) {
    return f;
  }
  return f + b;
}

double dmin(double a, double b) {
  return a < b ? a : b;
}

double dmax(double a, double b) {
  return a > b ? a : b;
}

double dabs(double a) {
  return a < 0 ? -a : a;
}

ubool doubleIsCloseEx(double a, double b, double relTol, double absTol) {
  return dabs(a - b) <= dmax(relTol * dmax(dabs(a), dabs(b)), absTol);
}

/*
 * TODO: Consider using higher precision with an implicit high bit
 * for partsToF24 and f24ToParts.
 *
 * I am pretty sure we can store more precision in the same number of
 * bits, but I don't want to spend too much time on this right now.
 *
 * Also, I think ~14-bits of precision is good enough for now especially
 * given that this is primarily used with Rect to specify screen coordinates.
 */

float partsToF24(i8 exponent, i16 significand) {
  /* 16384 = 2 ** 14 */
  return (((float)significand) / 16384) * pow(2, exponent);
}

void f24ToParts(float value, i8 *exponent, i16 *significand) {
  /* 16384 = 2 ** 14 */
  int exp;
  *significand = (i16)(frexp(value, &exp) * 16384);
  *exponent = (i8)exp;
}
