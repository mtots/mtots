#include "mtots_class_vector.h"

#include <math.h>

#include "mtots_vm.h"

static Status implVectorStaticCall(i16 argc, Value *argv, Value *out) {
  float x = argc > 0 && !isNil(argv[0]) ? asFloat(argv[0]) : 0.0f;
  float y = argc > 1 && !isNil(argv[1]) ? asFloat(argv[1]) : 0.0f;
  float z = argc > 2 && !isNil(argv[2]) ? asFloat(argv[2]) : 0.0f;
  *out = valVector(newVector(x, y, z));
  return STATUS_OK;
}

static CFunction funcVectorStaticCall = {implVectorStaticCall, "__call__", 0, 3};

static Status implVectorGetattr(i16 argc, Value *argv, Value *out) {
  Vector vector = asVector(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.xString) {
    *out = valNumber(vector.x);
  } else if (name == vm.yString) {
    *out = valNumber(vector.y);
  } else if (name == vm.zString) {
    *out = valNumber(vector.z);
  } else {
    fieldNotFoundError(argv[-1], name->chars);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcVectorGetattr = {implVectorGetattr, "__getattr__", 1};

static Status implVectorAdd(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  Vector b = asVector(argv[0]);
  Vector c = newVector(a.x + b.x, a.y + b.y, a.z + b.z);
  *out = valVector(c);
  return STATUS_OK;
}

static CFunction funcVectorAdd = {implVectorAdd, "__add__", 1};

static Status implVectorSub(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  Vector b = asVector(argv[0]);
  Vector c = newVector(a.x - b.x, a.y - b.y, a.z - b.z);
  *out = valVector(c);
  return STATUS_OK;
}

static CFunction funcVectorSub = {implVectorSub, "__sub__", 1};

static Status implVectorMul(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  double b = asFloat(argv[0]);
  Vector c = newVector(a.x * b, a.y * b, a.z * b);
  *out = valVector(c);
  return STATUS_OK;
}

static CFunction funcVectorMul = {implVectorMul, "__mul__", 1};

static Status implVectorScale(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  double fx = asNumber(argv[0]);
  double fy = asNumber(argv[1]);
  double fz = argc > 2 && !isNil(argv[2]) ? asNumber(argv[2]) : 1.0f;
  *out = valVector(newVector(a.x * fx, a.y * fy, a.z * fz));
  return STATUS_OK;
}

static CFunction funcVectorScale = {implVectorScale, "scale", 2, 3};

static Status implVectorDot(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  Vector b = asVector(argv[0]);
  *out = valNumber(a.x * b.x + a.y * b.y + a.z * b.z);
  return STATUS_OK;
}

static CFunction funcVectorDot = {implVectorDot, "dot", 1};

static Status implVectorGetLength(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  double x = a.x;
  double y = a.y;
  double z = a.z;
  *out = valNumber(sqrt(x * x + y * y + z * z));
  return STATUS_OK;
}

static CFunction funcVectorGetLength = {implVectorGetLength, "getLength"};

static Status implVectorRotateX(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  double angle = asNumber(argv[0]);
  Vector c = argc > 1 && !isNil(argv[1]) ? asVector(argv[1]) : newVector(0, 0, 0);
  double cost = cos(angle);
  double sint = sin(angle);
  a = newVector(a.x - c.x, a.y - c.y, a.z - c.z);
  a = newVector(a.x, cost * a.y - sint * a.z, sint * a.y + cost * a.z);
  a = newVector(a.x + c.x, a.y + c.y, a.z + c.z);
  *out = valVector(a);
  return STATUS_OK;
}

static CFunction funcVectorRotateX = {implVectorRotateX, "rotateX", 1, 2};

static Status implVectorRotateY(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  double angle = asNumber(argv[0]);
  Vector c = argc > 1 && !isNil(argv[1]) ? asVector(argv[1]) : newVector(0, 0, 0);
  double cost = cos(angle);
  double sint = sin(angle);
  a = newVector(a.x - c.x, a.y - c.y, a.z - c.z);
  a = newVector(cost * a.x + sint * a.z, a.y, -sint * a.x + cost * a.z);
  a = newVector(a.x + c.x, a.y + c.y, a.z + c.z);
  *out = valVector(a);
  return STATUS_OK;
}

static CFunction funcVectorRotateY = {implVectorRotateY, "rotateY", 1, 2};

static Status implVectorRotateZ(i16 argc, Value *argv, Value *out) {
  Vector a = asVector(argv[-1]);
  double angle = asNumber(argv[0]);
  Vector c = argc > 1 && !isNil(argv[1]) ? asVector(argv[1]) : newVector(0, 0, 0);
  double cost = cos(angle);
  double sint = sin(angle);
  a = newVector(a.x - c.x, a.y - c.y, a.z - c.z);
  a = newVector(cost * a.x - sint * a.y, sint * a.x + cost * a.y, a.z);
  a = newVector(a.x + c.x, a.y + c.y, a.z + c.z);
  *out = valVector(a);
  return STATUS_OK;
}

static CFunction funcVectorRotateZ = {implVectorRotateZ, "rotateZ", 1, 2};

static CFunction funcVectorRotate = {implVectorRotateZ, "rotate", 1, 2};

void initVectorClass(void) {
  CFunction *staticMethods[] = {
      &funcVectorStaticCall,
      NULL,
  };
  CFunction *methods[] = {
      &funcVectorGetattr,
      &funcVectorAdd,
      &funcVectorSub,
      &funcVectorMul,
      &funcVectorScale,
      &funcVectorDot,
      &funcVectorGetLength,
      &funcVectorRotateX,
      &funcVectorRotateY,
      &funcVectorRotateZ,
      &funcVectorRotate,
      NULL,
  };
  newBuiltinClass("Vector", &vm.vectorClass, methods, staticMethods);
}
