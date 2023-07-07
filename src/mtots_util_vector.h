#ifndef mtots_util_vector_h
#define mtots_util_vector_h

#include "mtots_common.h"

typedef struct Vector {
  float x;
  float y;
  float z;
} Vector;

Vector newVector(float x, float y, float z);
ubool vectorEquals(Vector a, Vector b);
ubool vectorIsCloseEx(Vector a, Vector b, double relTol, double absTol);
Vector vectorAdd(Vector a, Vector b);
Vector vectorSub(Vector a, Vector b);
Vector vectorScale(Vector a, double scale);
double vectorDot(Vector a, Vector b);
Vector vectorRotateX(Vector v, double theta);
Vector vectorRotateY(Vector v, double theta);
Vector vectorRotateZ(Vector v, double theta);

#endif/*mtots_util_vector_h*/
