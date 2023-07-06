#ifndef mtots_util_matrix_h
#define mtots_util_matrix_h

#include "mtots_util_vector.h"

/*
 * 4x4 Matrix
 *
 * The dimension 4x4 was chosen so that matrices can hold
 * 3D affine transformations
 *
 * Data stored in row major order
 */
typedef struct Matrix {
  float row[4][4];
} Matrix;

void initMatrixFromValues(Matrix *matrix, const float *values);
void initZeroMatrix(Matrix *matrix);
void initIdentityMatrix(Matrix *matrix);
void initMatrixTranslate(Matrix *m, Vector v);
void initMatrixScale(Matrix *m, Vector v);
void initMatrixRotateX(Matrix *m, double angle);
void initMatrixRotateY(Matrix *m, double angle);
void initMatrixRotateZ(Matrix *m, double angle);
void initMatrixFlipX(Matrix *m);
void initMatrixFlipY(Matrix *m);
void initMatrixFlipZ(Matrix *m);
void matrixISMul(Matrix *a, double factor);
void matrixIAdd(Matrix *a, const Matrix *b);
void matrixISub(Matrix *a, const Matrix *b);
void matrixIMul(Matrix *a, const Matrix *b);
void matrixITranspose(Matrix *m);
void matrixITranslate(Matrix *m, Vector v);
void matrixIScale(Matrix *m, Vector v);
void matrixIRotateX(Matrix *m, double angle);
void matrixIRotateY(Matrix *m, double angle);
void matrixIRotateZ(Matrix *m, double angle);
void matrixIFlipX(Matrix *m);
void matrixIFlipY(Matrix *m);
void matrixIFlipZ(Matrix *m);
ubool matrixIInverse(Matrix *z);
ubool matrixIPow(Matrix *z, i32 exponent);
double matrixDet2x2(const Matrix *m);
double matrixDet3x3(const Matrix *m);
double matrixDet(const Matrix *z);    /* 4D matrix */
Vector matrixApply(const Matrix *m, Vector v);

#endif/*mtots_util_matrix_h*/
