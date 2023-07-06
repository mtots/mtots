#include "mtots_class_matrix.h"

#include "mtots_vm.h"

NativeObjectDescriptor descriptorMatrix = {
  nopBlacken, nopFree, sizeof(ObjMatrix), "Matrix"
};

static TypePattern argsMatrices[] = {
  { TYPE_PATTERN_NATIVE, &descriptorMatrix },
  { TYPE_PATTERN_NATIVE, &descriptorMatrix },
  { TYPE_PATTERN_NATIVE, &descriptorMatrix },
  { TYPE_PATTERN_NATIVE, &descriptorMatrix },
};

Value MATRIX_VAL(ObjMatrix *matrix) {
  return OBJ_VAL_EXPLICIT((Obj*)matrix);
}

ObjMatrix *allocMatrix() {
  return NEW_NATIVE(ObjMatrix, &descriptorMatrix);
}

ObjMatrix *newZeroMatrix() {
  ObjMatrix *matrix = allocMatrix();
  initZeroMatrix(&matrix->handle);
  return matrix;
}

ObjMatrix *newIdentityMatrix() {
  ObjMatrix *matrix = allocMatrix();
  initIdentityMatrix(&matrix->handle);
  return matrix;
}

ObjMatrix *newMatrixFromValues(float *values) {
  ObjMatrix *matrix = allocMatrix();
  initMatrixFromValues(&matrix->handle, values);
  return matrix;
}

static ubool implMatrixStaticCall(i16 argc, Value *args, Value *out) {
  float values[4][4] = {0};
  size_t i;
  ObjList *outer = AS_LIST(args[0]);
  for (i = 0; i < 4 && i < outer->length; i++) {
    ObjList *inner = AS_LIST(outer->buffer[i]);
    size_t j;
    for (j = 0; j < 4 && j < inner->length; j++) {
      values[i][j] = AS_NUMBER(inner->buffer[j]);
    }
  }
  *out = MATRIX_VAL(newMatrixFromValues(&values[0][0]));
  return UTRUE;
}

static TypePattern argsMatrixStaticCall[] = {
  { TYPE_PATTERN_LIST_LIST_NUMBER },
};

static CFunction funcMatrixStaticCall = {
  implMatrixStaticCall, "__call__", 1, 0, argsMatrixStaticCall
};

static ubool implMatrixStaticFromColumnValues(i16 argc, Value *args, Value *out) {
  float values[4][4] = {0};
  size_t i;
  ObjList *outer = AS_LIST(args[0]);
  for (i = 0; i < 4 && i < outer->length; i++) {
    ObjList *inner = AS_LIST(outer->buffer[i]);
    size_t j;
    for (j = 0; j < 4 && j < inner->length; j++) {
      values[j][i] = AS_NUMBER(inner->buffer[j]);
    }
  }
  *out = MATRIX_VAL(newMatrixFromValues(&values[0][0]));
  return UTRUE;
}

static TypePattern argsMatrixStaticFromColumnValues[] = {
  { TYPE_PATTERN_LIST_LIST_NUMBER },
};

static CFunction funcMatrixStaticFromColumnValues = {
  implMatrixStaticFromColumnValues, "fromColumnValues", 1, 0, argsMatrixStaticFromColumnValues
};

static ubool implMatrixStaticFromColumns(i16 argc, Value *args, Value *out) {
  float values[4][4] = {0};
  size_t i;
  values[2][2] = values[3][3] = 1;
  for (i = 0; i < 4 && i < argc; i++) {
    if (!IS_NIL(args[i])) {
      Vector column = AS_VECTOR(args[i]);
      values[0][i] = column.x;
      values[1][i] = column.y;
      values[2][i] = column.z;
    }
  }
  if (argc < 3 || IS_NIL(args[2])) {
    values[2][2] = 1;
  }
  values[3][3] = 1;
  *out = MATRIX_VAL(newMatrixFromValues(&values[0][0]));
  return UTRUE;
}

static TypePattern argsMatrixStaticFromColumns[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR_OR_NIL },
  { TYPE_PATTERN_VECTOR_OR_NIL },
};

static CFunction funcMatrixStaticFromColumns = {
  implMatrixStaticFromColumns, "fromColumns", 2, 4, argsMatrixStaticFromColumns
};

static ubool implMatrixStaticFromRows(i16 argc, Value *args, Value *out) {
  float values[4][4] = {0};
  size_t i;
  values[2][2] = values[3][3] = 1;
  for (i = 0; i < 4 && i < argc; i++) {
    if (!IS_NIL(args[i])) {
      Vector row = AS_VECTOR(args[i]);
      values[i][0] = row.x;
      values[i][1] = row.y;
      values[i][2] = row.z;
    }
  }
  *out = MATRIX_VAL(newMatrixFromValues(&values[0][0]));
  return UTRUE;
}

static TypePattern argsMatrixStaticFromRows[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR_OR_NIL },
  { TYPE_PATTERN_VECTOR_OR_NIL },
};

static CFunction funcMatrixStaticFromRows = {
  implMatrixStaticFromRows, "fromRows", 2, 4, argsMatrixStaticFromRows
};

static ubool implMatrixStaticZero(i16 argc, Value *args, Value *out) {
  *out = MATRIX_VAL(newZeroMatrix());
  return UTRUE;
}

static CFunction funcMatrixStaticZero = { implMatrixStaticZero, "zero" };

static ubool implMatrixStaticOne(i16 argc, Value *args, Value *out) {
  *out = MATRIX_VAL(newIdentityMatrix());
  return UTRUE;
}

static CFunction funcMatrixStaticOne = { implMatrixStaticOne, "one" };

static ubool implMatrixClone(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  *out = MATRIX_VAL(newMatrixFromValues(&a->handle.row[0][0]));
  return UTRUE;
}

static CFunction funcMatrixClone = { implMatrixClone, "clone" };

static ubool implMatrixGet(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  size_t row = AS_INDEX(args[0], 4);
  size_t column = AS_INDEX(args[1], 4);
  *out = NUMBER_VAL(a->handle.row[row][column]);
  return UTRUE;
}

static CFunction funcMatrixGet = { implMatrixGet, "get", 2, 0, argsNumbers };

static ubool implMatrixSet(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  size_t row = AS_INDEX(args[0], 4);
  size_t column = AS_INDEX(args[1], 4);
  float value = AS_NUMBER(args[2]);
  a->handle.row[row][column] = value;
  return UTRUE;
}

static CFunction funcMatrixSet = { implMatrixSet, "set", 3, 0, argsNumbers };

static ubool implMatrixReset(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  initIdentityMatrix(&a->handle);
  return UTRUE;
}

static CFunction funcMatrixReset = { implMatrixReset, "reset" };

static ubool implMatrixIsmul(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  double factor = AS_NUMBER(args[0]);
  matrixISMul(&a->handle, factor);
  return UTRUE;
}

static CFunction funcMatrixIsmul = { implMatrixIsmul, "ismul", 1, 0, argsNumbers };

static ubool implMatrixIadd(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *b = AS_MATRIX(args[0]);
  matrixIAdd(&a->handle, &b->handle);
  return UTRUE;
}

static CFunction funcMatrixIadd = { implMatrixIadd, "iadd", 1, 0, argsMatrices };

static ubool implMatrixIsub(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *b = AS_MATRIX(args[0]);
  matrixISub(&a->handle, &b->handle);
  return UTRUE;
}

static CFunction funcMatrixIsub = { implMatrixIsub, "isub", 1, 0, argsMatrices };

static ubool implMatrixImul(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *b = AS_MATRIX(args[0]);
  matrixIMul(&a->handle, &b->handle);
  return UTRUE;
}

static CFunction funcMatrixImul = { implMatrixImul, "imul", 1, 0, argsMatrices };

static ubool implMatrixIpow(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  double exponentDouble = AS_NUMBER(args[0]);
  i32 exponent = AS_I32(args[0]);
  if (exponentDouble != (double)exponent) {
    runtimeError(
      "fractional matrix exponentiation is not supported (%f)",
      exponentDouble);
    return UFALSE;
  }
  if (!matrixIPow(&a->handle, exponent)) {
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcMatrixIpow = {
  implMatrixIpow, "ipow", 1, 0, argsNumbers
};

static ubool implMatrixItranspose(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  matrixITranspose(&a->handle);
  return UTRUE;
}

static CFunction funcMatrixItranspose = { implMatrixItranspose, "itranspose" };

static ubool implMatrixSmul(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  double factor = AS_NUMBER(args[0]);
  ObjMatrix *ret = newMatrixFromValues(&a->handle.row[0][0]);
  matrixISMul(&ret->handle, factor);
  *out = MATRIX_VAL(ret);
  return UTRUE;
}

static CFunction funcMatrixSmul = { implMatrixSmul, "smul", 1, 0, argsNumbers };

static ubool implMatrixAdd(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *b = AS_MATRIX(args[0]);
  ObjMatrix *ret = newMatrixFromValues(&a->handle.row[0][0]);
  matrixIAdd(&ret->handle, &b->handle);
  *out = MATRIX_VAL(ret);
  return UTRUE;
}

static CFunction funcMatrixAdd = { implMatrixAdd, "__add__", 1, 0, argsMatrices };

static ubool implMatrixSub(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *b = AS_MATRIX(args[0]);
  ObjMatrix *ret = newMatrixFromValues(&a->handle.row[0][0]);
  matrixISub(&ret->handle, &b->handle);
  *out = MATRIX_VAL(ret);
  return UTRUE;
}

static CFunction funcMatrixSub = { implMatrixSub, "__sub__", 1, 0, argsMatrices };

static ubool implMatrixMul(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *b = AS_MATRIX(args[0]);
  ObjMatrix *ret = newMatrixFromValues(&a->handle.row[0][0]);
  matrixIMul(&ret->handle, &b->handle);
  *out = MATRIX_VAL(ret);
  return UTRUE;
}

static CFunction funcMatrixMul = { implMatrixMul, "__mul__", 1, 0, argsMatrices };

static ubool implMatrixPow(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *ret = newMatrixFromValues(&a->handle.row[0][0]);
  double exponentDouble = AS_NUMBER(args[0]);
  i32 exponent = AS_I32(args[0]);
  if (exponentDouble != (double)exponent) {
    runtimeError(
      "fractional matrix exponentiation is not supported (%f)",
      exponentDouble);
    return UFALSE;
  }
  if (!matrixIPow(&ret->handle, exponent)) {
    return UFALSE;
  }
  *out = MATRIX_VAL(ret);
  return UTRUE;
}

static CFunction funcMatrixPow = {
  implMatrixPow, "__pow__", 1, 0, argsNumbers
};

static ubool implMatrixTranspose(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  ObjMatrix *ret = newMatrixFromValues(&a->handle.row[0][0]);
  matrixITranspose(&ret->handle);
  *out = MATRIX_VAL(ret);
  return UTRUE;
}

static CFunction funcMatrixTranspose = { implMatrixTranspose, "transpose" };

static ubool implMatrixDet2x2(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  *out = NUMBER_VAL(matrixDet2x2(&a->handle));
  return UTRUE;
}

static CFunction funcMatrixDet2x2 = { implMatrixDet2x2, "det2x2" };

static ubool implMatrixDet3x3(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  *out = NUMBER_VAL(matrixDet3x3(&a->handle));
  return UTRUE;
}

static CFunction funcMatrixDet3x3 = { implMatrixDet3x3, "det3x3" };

static ubool implMatrixDet(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  *out = NUMBER_VAL(matrixDet(&a->handle));
  return UTRUE;
}

static CFunction funcMatrixDet = { implMatrixDet, "det" };

static ubool implMatrixApply(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  Vector v = AS_VECTOR(args[0]);
  *out = VECTOR_VAL(matrixApply(&a->handle, v));
  return UTRUE;
}

static TypePattern argsMatrixApply[] = {
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcMatrixApply = { implMatrixApply, "apply", 1, 0, argsMatrixApply };

static ubool implMatrixTranslate(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  matrixITranslate(&a->handle, AS_VECTOR(args[0]));
  return UTRUE;
}

static TypePattern argsMatrixTranslate[] = {
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcMatrixTranslate = {
  implMatrixTranslate, "translate", 1, 0, argsMatrixTranslate
};

static ubool implMatrixScaleX(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  matrixIScale(&a->handle, newVector(AS_NUMBER(args[0]), 1, 1));
  return UTRUE;
}

static CFunction funcMatrixScaleX = {
  implMatrixScaleX, "scaleX", 1, 0, argsNumbers
};

static ubool implMatrixScaleY(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  matrixIScale(&a->handle, newVector(1, AS_NUMBER(args[0]), 1));
  return UTRUE;
}

static CFunction funcMatrixScaleY = {
  implMatrixScaleY, "scaleY", 1, 0, argsNumbers
};

static ubool implMatrixScaleZ(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  matrixIScale(&a->handle, newVector(1, 1, AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcMatrixScaleZ = {
  implMatrixScaleZ, "scaleZ", 1, 0, argsNumbers
};

static TypePattern argsMatrixRotate[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_VECTOR_OR_NIL },
};

static ubool implMatrixRotateX(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  double angle = AS_NUMBER(args[0]);
  if (argc > 1 && IS_VECTOR(args[1])) {
    Vector v = AS_VECTOR(args[1]);
    matrixITranslate(&a->handle, newVector(-v.x, -v.y, -v.z));
    matrixIRotateX(&a->handle, angle);
    matrixITranslate(&a->handle, v);
  } else {
    matrixIRotateX(&a->handle, angle);
  }
  return UTRUE;
}

static CFunction funcMatrixRotateX = {
  implMatrixRotateX, "rotateX", 1, 2, argsMatrixRotate
};

static ubool implMatrixRotateY(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  double angle = AS_NUMBER(args[0]);
  if (argc > 1 && IS_VECTOR(args[1])) {
    Vector v = AS_VECTOR(args[1]);
    matrixITranslate(&a->handle, newVector(-v.x, -v.y, -v.z));
    matrixIRotateY(&a->handle, angle);
    matrixITranslate(&a->handle, v);
  } else {
    matrixIRotateY(&a->handle, angle);
  }
  return UTRUE;
}

static CFunction funcMatrixRotateY = {
  implMatrixRotateY, "rotateY",  1, 2, argsMatrixRotate
};

static ubool implMatrixRotateZ(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  double angle = AS_NUMBER(args[0]);
  if (argc > 1 && IS_VECTOR(args[1])) {
    Vector v = AS_VECTOR(args[1]);
    matrixITranslate(&a->handle, newVector(-v.x, -v.y, -v.z));
    matrixIRotateZ(&a->handle, angle);
    matrixITranslate(&a->handle, v);
  } else {
    matrixIRotateZ(&a->handle, angle);
  }
  return UTRUE;
}

static CFunction funcMatrixRotateZ = {
  implMatrixRotateZ, "rotateZ",  1, 2, argsMatrixRotate
};

static ubool implMatrixRepr(i16 argc, Value *args, Value *out) {
  ObjMatrix *a = AS_MATRIX(args[-1]);
  Buffer buf;
  size_t i, j;
  initBuffer(&buf);
  bputstr(&buf, "Matrix([");
  for (i = 0; i < 4; i++) {
    if (i > 0) {
      bputstr(&buf, ", ");
    }
    bputstr(&buf, "[");
    for (j = 0; j < 4; j++) {
      if (j > 0) {
        bputstr(&buf, ", ");
      }
      bputnumber(&buf, a->handle.row[i][j]);
    }
    bputstr(&buf, "]");
  }
  bputstr(&buf, "])");
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction funcMatrixRepr = { implMatrixRepr, "__repr__" };

void initMatrixClass() {
  CFunction *staticMethods[] = {
    &funcMatrixStaticCall,
    &funcMatrixStaticFromColumnValues,
    &funcMatrixStaticFromColumns,
    &funcMatrixStaticFromRows,
    &funcMatrixStaticZero,
    &funcMatrixStaticOne,
    NULL,
  };
  CFunction *methods[] = {
    &funcMatrixClone,
    &funcMatrixGet,
    &funcMatrixSet,
    &funcMatrixReset,
    &funcMatrixIsmul,
    &funcMatrixIadd,
    &funcMatrixIsub,
    &funcMatrixImul,
    &funcMatrixIpow,
    &funcMatrixItranspose,
    &funcMatrixSmul,
    &funcMatrixAdd,
    &funcMatrixSub,
    &funcMatrixMul,
    &funcMatrixPow,
    &funcMatrixTranspose,
    &funcMatrixDet2x2,
    &funcMatrixDet3x3,
    &funcMatrixDet,
    &funcMatrixApply,
    &funcMatrixTranslate,
    &funcMatrixScaleX,
    &funcMatrixScaleY,
    &funcMatrixScaleZ,
    &funcMatrixRotateX,
    &funcMatrixRotateY,
    &funcMatrixRotateZ,
    &funcMatrixRepr,
    NULL,
  };
  newNativeClass(NULL, &descriptorMatrix, methods, staticMethods);
}
