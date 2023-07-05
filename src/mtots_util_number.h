#ifndef mtots_util_number_h
#define mtots_util_number_h

#include "mtots_common.h"

#define PI    3.14159265358979323846264338327950288
#define TAU  (2 * PI)

ubool doubleIsI32(double value);

/* Like fmod, but the sign of the return value matches `b` instead of `a` */
double mfmod(double a, double b);

/* Get the minimum of two doubles */
double dmin(double a, double b);

/* Get the maximum of two doubles */
double dmax(double a, double b);

/* Get the absolute value of a double value */
double dabs(double a);

#endif/*mtots_util_number_h*/
