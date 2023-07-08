#include "mtots_vm.h"
#include "mtots_class_vector.h"

static TypePattern argsVectors[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
};

static ubool implVectorStaticCall(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(newVector(
    AS_NUMBER(args[0]),
    AS_NUMBER(args[1]),
    argc > 2 ? AS_NUMBER(args[2]) : 0));
  return UTRUE;
}

static CFunction funcVectorStaticCall = {
  implVectorStaticCall, "__call__", 2, 3, argsNumbers
};

static ubool implVectorGetattr(i16 argc, Value *args, Value *out) {
  Vector vector = AS_VECTOR(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.xString) {
    *out = NUMBER_VAL(vector.x);
  } else if (name == vm.yString) {
    *out = NUMBER_VAL(vector.y);
  } else if (name == vm.zString) {
    *out = NUMBER_VAL(vector.z);
  } else {
    runtimeError("Field %s not found on Vector", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcVectorGetattr = {
  implVectorGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implVectorAdd(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(vectorAdd(AS_VECTOR(args[-1]), AS_VECTOR(args[0])));
  return UTRUE;
}

static CFunction funcVectorAdd = {
  implVectorAdd, "__add__", 1, 0, argsVectors,
};

static ubool implVectorSub(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(vectorSub(AS_VECTOR(args[-1]), AS_VECTOR(args[0])));
  return UTRUE;
}

static CFunction funcVectorSub = {
  implVectorSub, "__sub__", 1, 0, argsVectors,
};

static ubool implVectorDot(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(vectorDot(AS_VECTOR(args[-1]), AS_VECTOR(args[0])));
  return UTRUE;
}

static CFunction funcVectorDot = { implVectorDot, "dot", 1, 0, argsVectors };

static ubool implVectorMul(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(vectorScale(AS_VECTOR(args[-1]), AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcVectorMul = {
  implVectorMul, "__mul__", 1, 0, argsNumbers
};

static ubool implVectorRotateX(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(vectorRotateX(AS_VECTOR(args[-1]), AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcVectorRotateX = {
  implVectorRotateX, "rotateX", 1, 0, argsNumbers,
};

static ubool implVectorRotateY(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(vectorRotateY(AS_VECTOR(args[-1]), AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcVectorRotateY = {
  implVectorRotateY, "rotateY", 1, 0, argsNumbers,
};

static ubool implVectorRotateZ(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(vectorRotateZ(AS_VECTOR(args[-1]), AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcVectorRotateZ = {
  implVectorRotateZ, "rotateZ", 1, 0, argsNumbers,
};

void initVectorClass(void) {
  CFunction *staticMethods[] = {
    &funcVectorStaticCall,
    NULL,
  };
  CFunction *methods[] = {
    &funcVectorGetattr,
    &funcVectorAdd,
    &funcVectorSub,
    &funcVectorDot,
    &funcVectorMul,
    &funcVectorRotateX,
    &funcVectorRotateY,
    &funcVectorRotateZ,
    NULL,
  };
  newBuiltinClass(
    "Vector",
    &vm.vectorClass,
    TYPE_PATTERN_VECTOR,
    methods,
    staticMethods);
}
