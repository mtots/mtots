#include "mtots_util_matrix.h"

#include "mtots_util_error.h"

#include <string.h>
#include <math.h>

static const float zeroMatrixValues[4][4] = {
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
};

static const float identityMatrixValues[4][4] = {
  {1, 0, 0, 0},
  {0, 1, 0, 0},
  {0, 0, 1, 0},
  {0, 0, 0, 1},
};

void initMatrixFromValues(Matrix *matrix, const float *values) {
  memcpy(matrix->row, values, sizeof(float) * 4 * 4);
}

void initZeroMatrix(Matrix *matrix) {
  initMatrixFromValues(matrix, &zeroMatrixValues[0][0]);
}

void initIdentityMatrix(Matrix *matrix) {
  initMatrixFromValues(matrix, &identityMatrixValues[0][0]);
}

void initMatrixTranslate(Matrix *m, Vector v) {
  initIdentityMatrix(m);
  m->row[0][3] = v.x;
  m->row[1][3] = v.y;
  m->row[2][3] = v.z;
}

void initMatrixRotateX(Matrix *m, double angle) {
  double cost = cos(angle), sint = sin(angle);
  initIdentityMatrix(m);
  m->row[1][1] = cost;
  m->row[1][2] = -sint;
  m->row[2][1] = sint;
  m->row[2][2] = cost;
}

void initMatrixRotateY(Matrix *m, double angle) {
  double cost = cos(angle), sint = sin(angle);
  initIdentityMatrix(m);
  m->row[0][0] = cost;
  m->row[0][2] = sint;
  m->row[2][0] = -sint;
  m->row[2][2] = cost;
}

void initMatrixRotateZ(Matrix *m, double angle) {
  double cost = cos(angle), sint = sin(angle);
  initIdentityMatrix(m);
  m->row[0][0] = cost;
  m->row[0][1] = -sint;
  m->row[1][0] = sint;
  m->row[1][1] = cost;
}

/* in-place matrix scalar multiplication */
void matrixIScale(Matrix *a, double factor) {
  size_t i, j;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      a->row[i][j] *= factor;
    }
  }
}

/* in-place matrix addition */
void matrixIAdd(Matrix *a, const Matrix *b) {
  size_t i, j;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      a->row[i][j] += b->row[i][j];
    }
  }
}

/* in-place matrix subtraction */
void matrixISub(Matrix *a, const Matrix *b) {
  size_t i, j;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      a->row[i][j] -= b->row[i][j];
    }
  }
}

/* in-place matrix multiplication
 *
 * This is in-place in the sense that the first operand is modified.
 * Some extra space is still allocated on the stack to perform the
 * operation.
 */
void matrixIMul(Matrix *a, const Matrix *b) {
  size_t i, j, k;
  float values[4][4] = {0};
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      for (k = 0; k < 4; k++) {
        values[i][j] += a->row[i][k] * b->row[k][j];
      }
    }
  }
  initMatrixFromValues(a, &values[0][0]);
}

/* in-place matrix transpose */
void matrixITranspose(Matrix *m) {
  size_t i, k;
  for (i = 0; i < 3; i++) {
    for (k = i + 1; k < 4; k++) {
      float tmp = m->row[i][k];
      m->row[i][k] = m->row[k][i];
      m->row[k][i] = tmp;
    }
  }
}

void matrixITranslate(Matrix *m, Vector v) {
  Matrix t;
  initMatrixTranslate(&t, v);
  matrixIMul(&t, m);
  *m = t;
}

void matrixIRotateX(Matrix *m, double angle) {
  Matrix r;
  initMatrixRotateX(&r, angle);
  matrixIMul(&r, m);
  *m = r;
}

void matrixIRotateY(Matrix *m, double angle) {
  Matrix r;
  initMatrixRotateY(&r, angle);
  matrixIMul(&r, m);
  *m = r;
}

void matrixIRotateZ(Matrix *m, double angle) {
  Matrix r;
  initMatrixRotateZ(&r, angle);
  matrixIMul(&r, m);
  *m = r;
}

/*
 * Computes the inverse matrix inplace
 *
 * If the matrix is non-invertible, throws a runtime error.
 * Otherwise, `m` is updated to its inverse
 */
ubool matrixIInverse(Matrix *z) {
  double det = matrixDet(z), idet;
  float values[16];
  float a = z->row[0][0];
  float b = z->row[0][1];
  float c = z->row[0][2];
  float d = z->row[0][3];
  float e = z->row[1][0];
  float f = z->row[1][1];
  float g = z->row[1][2];
  float h = z->row[1][3];
  float i = z->row[2][0];
  float j = z->row[2][1];
  float k = z->row[2][2];
  float l = z->row[2][3];
  float m = z->row[3][0];
  float n = z->row[3][1];
  float o = z->row[3][2];
  float p = z->row[3][3];
  if (det == 0) {
    runtimeError("Tried to invert a non-invertible matrix");
    return UFALSE;
  }
  idet = 1/det;
  values[0] = (f*k*p-f*l*o-g*j*p+g*l*n+h*j*o-h*k*n) * idet;
  values[1] = (-b*k*p+b*l*o+c*j*p-c*l*n-d*j*o+d*k*n) * idet;
  values[2] = (b*g*p-b*h*o-c*f*p+c*h*n+d*f*o-d*g*n) * idet;
  values[3] = (-b*g*l+b*h*k+c*f*l-c*h*j-d*f*k+d*g*j) * idet;
  values[4] = (-e*k*p+e*l*o+g*i*p-g*l*m-h*i*o+h*k*m) * idet;
  values[5] = (a*k*p-a*l*o-c*i*p+c*l*m+d*i*o-d*k*m) * idet;
  values[6] = (-a*g*p+a*h*o+c*e*p-c*h*m-d*e*o+d*g*m) * idet;
  values[7] = (a*g*l-a*h*k-c*e*l+c*h*i+d*e*k-d*g*i) * idet;
  values[8] = (e*j*p-e*l*n-f*i*p+f*l*m+h*i*n-h*j*m) * idet;
  values[9] = (-a*j*p+a*l*n+b*i*p-b*l*m-d*i*n+d*j*m) * idet;
  values[10] = (a*f*p-a*h*n-b*e*p+b*h*m+d*e*n-d*f*m) * idet;
  values[11] = (-a*f*l+a*h*j+b*e*l-b*h*i-d*e*j+d*f*i) * idet;
  values[12] = (-e*j*o+e*k*n+f*i*o-f*k*m-g*i*n+g*j*m) * idet;
  values[13] = (a*j*o-a*k*n-b*i*o+b*k*m+c*i*n-c*j*m) * idet;
  values[14] = (-a*f*o+a*g*n+b*e*o-b*g*m-c*e*n+c*f*m) * idet;
  values[15] = (a*f*k-a*g*j-b*e*k+b*g*i+c*e*j-c*f*i) * idet;
  initMatrixFromValues(z, &values[0]);
  return UTRUE;
}

/*
 * Computes the power of the given matrix inplace
 *
 * If the matrix is non-invertible and the exponent is negative,
 * throws an error
 */
ubool matrixIPow(Matrix *z, i32 exponent) {
  Matrix m = *z;
  u32 e = (u32)(exponent < 0 ? (-exponent) : exponent);
  initIdentityMatrix(z);
  if (e == 0) {
    return UTRUE;
  }
  if (exponent < 0) {
    if (!matrixIInverse(&m)) {
      return UFALSE;
    }
  }
  for (; e; e /= 2) {
    if (e & 1) {
      matrixIMul(z, &m);
    }
    matrixIMul(&m, &m);
  }
  return UTRUE;
}

double matrixDet2x2(const Matrix *m) {
  return m->row[0][0] * m->row[1][1] - m->row[0][1] * m->row[1][0];
}

double matrixDet3x3(const Matrix *m) {
  float a = m->row[0][0];
  float b = m->row[0][1];
  float c = m->row[0][2];
  float d = m->row[1][0];
  float e = m->row[1][1];
  float f = m->row[1][2];
  float g = m->row[2][0];
  float h = m->row[2][1];
  float i = m->row[2][2];
  return a * e * i +
         b * f * g +
         c * d * h -
         c * e * g -
         b * d * i -
         a * f * h;
}

double matrixDet(const Matrix *z) {
  float a = z->row[0][0];
  float b = z->row[0][1];
  float c = z->row[0][2];
  float d = z->row[0][3];
  float e = z->row[1][0];
  float f = z->row[1][1];
  float g = z->row[1][2];
  float h = z->row[1][3];
  float i = z->row[2][0];
  float j = z->row[2][1];
  float k = z->row[2][2];
  float l = z->row[2][3];
  float m = z->row[3][0];
  float n = z->row[3][1];
  float o = z->row[3][2];
  float p = z->row[3][3];
  return a * f * k * p - a * f * l * o - a * g * j * p + a * g * l * n +
         a * h * j * o - a * h * k * n - b * e * k * p + b * e * l * o +
         b * g * i * p - b * g * l * m - b * h * i * o + b * h * k * m +
         c * e * j * p - c * e * l * n - c * f * i * p + c * f * l * m +
         c * h * i * n - c * h * j * m - d * e * j * o + d * e * k * n +
         d * f * i * o - d * f * k * m - d * g * i * n + d * g * j * m;
}

/*
 * Applies the transform represented by this matrix.
 *
 * This function interprets the vector as a column matrix, and performs
 * matrix multiplication.
 *
 * Since the vector only has 3 components, the fourth component is
 * assumed to be 1 to allow for affine transformations.
 */
Vector matrixApply(const Matrix *m, Vector v) {
  return newVector(
    m->row[0][0] * v.x + m->row[0][1] * v.y + m->row[0][2] * v.z + m->row[0][3],
    m->row[1][0] * v.x + m->row[1][1] * v.y + m->row[1][2] * v.z + m->row[1][3],
    m->row[2][0] * v.x + m->row[2][1] * v.y + m->row[2][2] * v.z + m->row[2][3]);
}
