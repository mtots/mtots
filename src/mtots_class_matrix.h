#ifndef mtots_class_matrix_h
#define mtots_class_matrix_h

#include "mtots_object.h"

#define AS_MATRIX(value) ((ObjMatrix*)AS_OBJ(value))
#define IS_MATRIX(value) (getNativeObjectDescriptor(value) == &descriptorMatrix)

typedef struct ObjMatrix {
  ObjNative obj;
  Matrix handle;
} ObjMatrix;

Value MATRIX_VAL(ObjMatrix *matrix);

ObjMatrix *allocMatrix();
ObjMatrix *newZeroMatrix();
ObjMatrix *newIdentityMatrix();
ObjMatrix *newMatrixFromValues(float *values);
void initMatrixClass();

extern NativeObjectDescriptor descriptorMatrix;

#endif/*mtots_class_matrix_h*/
