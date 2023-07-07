#include "mtots_util_vector.h"

#include "mtots_util_number.h"

#include <math.h>

Vector newVector(float x, float y, float z) {
  Vector v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

ubool vectorEquals(Vector a, Vector b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

ubool vectorIsCloseEx(Vector a, Vector b, double relTol, double absTol) {
  return doubleIsCloseEx(a.x, b.x, relTol, absTol) &&
    doubleIsCloseEx(a.y, b.y, relTol, absTol) &&
    doubleIsCloseEx(a.z, b.z, relTol, absTol);
}

Vector vectorAdd(Vector a, Vector b) {
  return newVector(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vector vectorSub(Vector a, Vector b) {
  return newVector(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vector vectorScale(Vector a, double scale) {
  return newVector(a.x * scale, a.y * scale, a.z * scale);
}

double vectorDot(Vector a, Vector b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/*
 * Rotate `v` counterclockwise by `theta` radians about the X axis.
 * (standard right handed cartesian coordinate system is assumed)
 */
Vector vectorRotateX(Vector v, double theta) {
  double cost = cos(theta), sint = sin(theta);
  return newVector(
    v.x, cost * v.y - sint * v.z, sint * v.y + cost * v.z);
}

/*
 * Rotate `v` counterclockwise by `theta` radians about the Y axis.
 * (standard right handed cartesian coordinate system is assumed)
 */
Vector vectorRotateY(Vector v, double theta) {
  double cost = cos(theta), sint = sin(theta);
  return newVector(
    cost * v.x + sint * v.z, v.y, -sint * v.x + cost * v.z);
}

/*
 * Rotate `v` counterclockwise by `theta` radians about the Z axis.
 * (standard right handed cartesian coordinate system is assumed)
 *
 * In a 2D coordinate system where the positive y-axis points down,
 * this function will rotate `v` clockwise.
 */
Vector vectorRotateZ(Vector v, double theta) {
  double cost = cos(theta), sint = sin(theta);
  return newVector(
    cost * v.x - sint * v.y, sint * v.x + cost * v.y, v.z);
}
