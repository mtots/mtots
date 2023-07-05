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
