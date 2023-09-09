#include "mtots_vm.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_assumptions.h"
#include "mtots_class_buffer.h"
#include "mtots_class_class.h"
#include "mtots_class_dict.h"
#include "mtots_class_frozendict.h"
#include "mtots_class_frozenlist.h"
#include "mtots_class_list.h"
#include "mtots_class_number.h"
#include "mtots_class_str.h"
#include "mtots_globals.h"
#include "mtots_modules.h"
#include "mtots_parser.h"

VM vm;

static ubool invoke(String *name, i16 argCount);

static void resetStack(void) {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
  vm.trySnapshotsCount = 0;
}

static void printStackToStringBuffer(Buffer *out) {
  i16 i;
  for (i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjThunk *thunk = frame->closure->thunk;
    size_t instruction = frame->ip - thunk->chunk.code - 1;
    bprintf(
        out, "[line %d] in ", thunk->chunk.lines[instruction]);
    if (thunk->name == NULL) {
      if (thunk->moduleName == NULL) {
        bprintf(out, "[script]\n");
      } else {
        bprintf(out, "%s\n", thunk->moduleName->chars);
      }
    } else {
      if (thunk->moduleName == NULL) {
        bprintf(out, "%s()\n", thunk->name->chars);
      } else {
        bprintf(out, "%s:%s()\n",
                thunk->moduleName->chars, thunk->name->chars);
      }
    }
  }
}

void defineGlobal(const char *name, Value value) {
  push(STRING_VAL(internString(name, strlen(name))));
  push(value);
  mapSetStr(&vm.globals, vm.stack[0].as.string, vm.stack[1]);
  pop();
  pop();
}

void addNativeModule(CFunction *func) {
  String *name;
  if (func->arity != 1) {
    panic("Native modules must accept 1 argument but got %d", func->arity);
  }
  name = internCString(func->name);
  push(STRING_VAL(name));
  if (!mapSetStr(&vm.nativeModuleThunks, name, CFUNCTION_VAL(func))) {
    panic("Native module %s is already defined", name->chars);
  }

  pop(); /* name */
}

static void initNoMethodClass(ObjClass **clsptr, const char *name) {
  newBuiltinClass(name, clsptr, NULL, NULL);
}

void initVM(void) {
  setErrorContextProvider(printStackToStringBuffer);
  checkAssumptions();
  initSpecialStrings();
  resetStack();
  initMemory(&vm.memory);
  vm.runOnFinish = NULL;
  vm.enableGCLogs = UFALSE;
  vm.enableMallocFreeLogs = UFALSE;
  vm.enableLogOnGC = UFALSE;
  vm.localGCPause = UFALSE;

  if (sizeof(Value) != 16) {
    panic("sizeof(Value) != 16 (got %lu)", (unsigned long)sizeof(Value));
  }

  vm.sentinelClass = NULL;
  vm.nilClass = NULL;
  vm.boolClass = NULL;
  vm.numberClass = NULL;
  vm.stringClass = NULL;
  vm.listClass = NULL;
  vm.frozenListClass = NULL;
  vm.dictClass = NULL;
  vm.frozenDictClass = NULL;
  vm.functionClass = NULL;
  vm.classClass = NULL;

  initMap(&vm.globals);
  initMap(&vm.modules);
  initMap(&vm.nativeModuleThunks);
  initMap(&vm.frozenLists);
  initMap(&vm.frozenDicts);

  vm.initString = internForeverCString("__init__");
  vm.iterString = internForeverCString("__iter__");
  vm.lenString = internForeverCString("__len__");
  vm.reprString = internForeverCString("__repr__");
  vm.addString = internForeverCString("__add__");
  vm.subString = internForeverCString("__sub__");
  vm.mulString = internForeverCString("__mul__");
  vm.divString = internForeverCString("__div__");
  vm.floordivString = internForeverCString("__floordiv__");
  vm.modString = internForeverCString("__mod__");
  vm.powString = internForeverCString("__pow__");
  vm.negString = internForeverCString("__neg__");
  vm.containsString = internForeverCString("__contains__");
  vm.nilString = internForeverCString("nil");
  vm.trueString = internForeverCString("true");
  vm.falseString = internForeverCString("false");
  vm.getitemString = internForeverCString("__getitem__");
  vm.setitemString = internForeverCString("__setitem__");
  vm.sliceString = internForeverCString("__slice__");
  vm.getattrString = internForeverCString("__getattr__");
  vm.setattrString = internForeverCString("__setattr__");
  vm.callString = internForeverCString("__call__");
  vm.redString = internForeverCString("red");
  vm.greenString = internForeverCString("green");
  vm.blueString = internForeverCString("blue");
  vm.alphaString = internForeverCString("alpha");
  vm.xString = internForeverCString("x");
  vm.yString = internForeverCString("y");
  vm.zString = internForeverCString("z");
  vm.widthString = internForeverCString("width");
  vm.heightString = internForeverCString("height");
  vm.minXString = internForeverCString("minX");
  vm.minYString = internForeverCString("minY");
  vm.maxXString = internForeverCString("maxX");
  vm.maxYString = internForeverCString("maxY");

  initNoMethodClass(&vm.sentinelClass, "Sentinel");

  initNoMethodClass(&vm.nilClass, "Nil");
  initNoMethodClass(&vm.boolClass, "Bool");
  initNumberClass();
  initStringClass();
  initBufferClass();
  initListClass();
  initFrozenListClass();
  initDictClass();
  initFrozenDictClass();
  initNoMethodClass(&vm.functionClass, "Function");
  initClassClass();

  initRangeClass();
  initRangeIteratorClass();
  initStringBuilderClass();

  defineDefaultGlobals();
  addNativeModules();
}

void freeVM(void) {
  freeMap(&vm.globals);
  freeMap(&vm.modules);
  freeMap(&vm.nativeModuleThunks);
  freeMap(&vm.frozenLists);
  freeMap(&vm.frozenDicts);
  freeObjects();
}

void push(Value value) {
  if (vm.stackTop + 1 > vm.stack + STACK_MAX) {
    panic("stack overflow");
  }
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop(void) {
  if (vm.stackTop <= vm.stack) {
    panic("stack underflow");
  }
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static ubool isIterator(Value value) {
  if (IS_OBJ(value)) {
    switch (AS_OBJ_UNSAFE(value)->type) {
      case OBJ_CLOSURE:
        return AS_CLOSURE_UNSAFE(value)->thunk->arity == 0;
      case OBJ_NATIVE: {
        CFunction *call = AS_NATIVE_UNSAFE(value)->descriptor->klass->call;
        return call && call->arity == 0;
      }
      default:
        break;
    }
  }
  return STATUS_ERROR;
}

static ubool callCFunction(CFunction *cfunc, i16 argCount) {
  Value result = NIL_VAL(), *argsStart;
  ubool status;
  if (cfunc->arity != argCount) {
    /* not an exact match for the arity
     * We need further checks */
    if (cfunc->maxArity) {
      /* we have optional args */
      if (argCount < cfunc->arity) {
        runtimeError(
            "Function %s expects at least %d arguments but got %d",
            cfunc->name, cfunc->arity, argCount);
        return STATUS_ERROR;
      } else if (argCount > cfunc->maxArity) {
        runtimeError(
            "Function %s expects at most %d arguments but got %d",
            cfunc->name, cfunc->maxArity, argCount);
        return STATUS_ERROR;
      }
      /* At this point we have argCount between
       * cfunc->arity and cfunc->maxArity */
    } else {
      runtimeError(
          "Function %s expects %d arguments but got %d",
          cfunc->name, cfunc->arity, argCount);
      return STATUS_ERROR;
    }
  }
  argsStart = vm.stackTop - argCount;
  status = cfunc->body(argCount, argsStart, &result);
  if (!status) {
    return STATUS_ERROR;
  }
  vm.stackTop -= argCount + 1;
  push(result);
  return STATUS_OK;
}

static Status setupCallClosure(ObjClosure *closure, i16 argCount) {
  CallFrame *frame;

  if (argCount < closure->thunk->arity &&
      argCount + closure->thunk->defaultArgsCount >=
          closure->thunk->arity) {
    i16 requiredArgCount = closure->thunk->arity - closure->thunk->defaultArgsCount;
    while (argCount < closure->thunk->arity) {
      push(closure->thunk->defaultArgs[argCount - requiredArgCount]);
      argCount++;
    }
  }

  if (argCount != closure->thunk->arity) {
    runtimeError(
        "Expected %d arguments but got %d",
        closure->thunk->arity, argCount);
    return STATUS_ERROR;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow");
    return STATUS_ERROR;
  }

  frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->thunk->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return STATUS_OK;
}

static Status callFunctionOrMethod(Value callable, i16 argCount, ubool consummate);

static Status setupOrCallValue(Value callee, i16 argCount) {
  return callFunctionOrMethod(callee, argCount, UFALSE);
}

static Status invokeFromClass(
    ObjClass *klass, String *name, i16 argCount) {
  Value method;
  if (!mapGetStr(&klass->methods, name, &method)) {
    runtimeError(
        "Method '%s' not found in '%s'",
        name->chars,
        klass->name->chars);
    return STATUS_ERROR;
  }
  return setupOrCallValue(method, argCount);
}

/* Prepares */
static ubool invoke(String *name, i16 argCount) {
  ObjClass *klass;
  Value receiver = peek(argCount);

  klass = getClassOfValue(receiver);
  if (klass == NULL) {
    runtimeError(
        "%s kind does not yet support method calls", getKindName(receiver));
    return STATUS_ERROR;
  }
  if (klass == vm.classClass) {
    /* For classes, we invoke static methods */
    ObjClass *cls;
    Value method;
    if (!IS_CLASS(receiver)) {
      panic("Class instance is not a Class (%s)", getKindName(receiver));
    }
    cls = AS_CLASS_UNSAFE(receiver);
    if (!mapGetStr(&cls->staticMethods, name, &method)) {
      runtimeError(
          "Static method '%s' not found in '%s'",
          name->chars,
          cls->name->chars);
      return STATUS_ERROR;
    }
    return setupOrCallValue(method, argCount);
  }

  return invokeFromClass(klass, name, argCount);
}

static ObjUpvalue *captureUpvalue(Value *local) {
  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm.openUpvalues;
  ObjUpvalue *createdUpvalue;

  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;
  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }
  return createdUpvalue;
}

void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(String *name) {
  Value method = peek(0);
  ObjClass *klass = AS_CLASS_UNSAFE(peek(1));
  mapSetStr(&klass->methods, name, method);
  pop();
}

static void defineStaticMethod(String *name) {
  Value method = peek(0);
  ObjClass *klass = AS_CLASS_UNSAFE(peek(1));
  mapSetStr(&klass->staticMethods, name, method);
  pop();
}

static ubool isFalsey(Value value) {
  return IS_NIL(value) ||
         (IS_BOOL(value) && !value.as.boolean) ||
         (IS_NUMBER(value) && value.as.number == 0);
}

static void concatenate(void) {
  String *result;
  String *b = peek(0).as.string;
  String *a = peek(1).as.string;
  size_t length = a->byteLength + b->byteLength;
  char *chars = malloc(sizeof(char) * (length + 1));
  memcpy(chars, a->chars, a->byteLength);
  memcpy(chars + a->byteLength, b->chars, b->byteLength);
  chars[length] = '\0';
  result = internOwnedString(chars, length);
  pop();
  pop();
  push(STRING_VAL(result));
}

static Status run(void) {
  i16 returnFrameCount = vm.frameCount - 1;
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  i16 trySnapShotCountAtStart = vm.trySnapshotsCount;

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
  (frame->ip += 2, (u16)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
  (frame->closure->thunk->chunk.constants.values[READ_SHORT()])
#define READ_STRING() (READ_CONSTANT().as.string)
#define RETURN_RUNTIME_ERROR()                                                \
  do {                                                                        \
    TrySnapshot *snap;                                                        \
    if (vm.trySnapshotsCount <= trySnapShotCountAtStart) return STATUS_ERROR; \
    snap = &vm.trySnapshots[--vm.trySnapshotsCount];                          \
    vm.stackTop = snap->stackTop;                                             \
    vm.frameCount = snap->frameCount;                                         \
    frame = &vm.frames[vm.frameCount - 1];                                    \
    frame->ip = snap->ip;                                                     \
    saveCurrentErrorString();                                                 \
    goto loop;                                                                \
  } while (0)
#define INVOKE(methodName, argCount)       \
  do {                                     \
    if (!invoke(methodName, argCount)) {   \
      RETURN_RUNTIME_ERROR();              \
    }                                      \
    frame = &vm.frames[vm.frameCount - 1]; \
  } while (0)
#define CALL(argCount)                     \
  do {                                     \
    i16 ac = argCount;                     \
    if (!setupOrCallValue(peek(ac), ac)) { \
      RETURN_RUNTIME_ERROR();              \
    }                                      \
    frame = &vm.frames[vm.frameCount - 1]; \
  } while (0)
#define BINARY_OP(opexpr, invokeStr)                 \
  do {                                               \
    if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {  \
      double b = pop().as.number;                    \
      double a = pop().as.number;                    \
      /* TODO: Forgive myself for this evil macro */ \
      push(NUMBER_VAL(opexpr));                      \
    } else {                                         \
      INVOKE(invokeStr, 1);                          \
    }                                                \
  } while (0)
#define BINARY_BITWISE_OP(opname, op)                          \
  do {                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {          \
      runtimeError(                                            \
          "Operands to %s must be numbers but got %s and %s",  \
          opname, getKindName(peek(1)), getKindName(peek(0))); \
      RETURN_RUNTIME_ERROR();                                  \
    }                                                          \
    {                                                          \
      u32 b = asU32Bits(pop());                                \
      u32 a = asU32Bits(pop());                                \
      push(NUMBER_VAL(a op b));                                \
    }                                                          \
  } while (0)

  for (;;) {
    u8 instruction;
  loop:
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL:
        push(NIL_VAL());
        break;
      case OP_TRUE:
        push(BOOL_VAL(1));
        break;
      case OP_FALSE:
        push(BOOL_VAL(0));
        break;
      case OP_POP:
        pop();
        break;
      case OP_GET_LOCAL: {
        u8 slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        u8 slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        String *name = READ_STRING();
        Value value;
        if (!mapGetStr(&frame->closure->module->fields, name, &value)) {
          runtimeError("Undefined variable '%s'", name->chars);
          RETURN_RUNTIME_ERROR();
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        String *name = READ_STRING();
        mapSetStr(&frame->closure->module->fields, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        String *name = READ_STRING();
        if (mapSetStr(&frame->closure->module->fields, name, peek(0))) {
          mapDeleteStr(&frame->closure->module->fields, name);
          runtimeError("Undefined variable '%s'", name->chars);
          RETURN_RUNTIME_ERROR();
        }
        break;
      }
      case OP_GET_UPVALUE: {
        u8 slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        u8 slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_FIELD: {
        String *name;
        Value value = NIL_VAL();

        if (IS_INSTANCE(peek(0))) {
          ObjInstance *instance;
          instance = AS_INSTANCE_UNSAFE(peek(0));
          name = READ_STRING();
          if (mapGetStr(&instance->fields, name, &value)) {
            pop(); /* Instance */
            push(value);
            break;
          }
          runtimeError(
              "Field '%s' not found in %s",
              name->chars, instance->klass->name->chars);
          RETURN_RUNTIME_ERROR();
        }

        if (IS_DICT(peek(0))) {
          ObjDict *d = AS_DICT_UNSAFE(peek(0));
          name = READ_STRING();
          if (mapGet(&d->map, STRING_VAL(name), &value)) {
            pop(); /* Instance */
            push(value);
            break;
          }
          runtimeError("Field '%s' not found in Dict", name->chars);
          RETURN_RUNTIME_ERROR();
        }

        if (IS_FROZEN_DICT(peek(0))) {
          ObjFrozenDict *d = AS_FROZEN_DICT_UNSAFE(peek(0));
          name = READ_STRING();
          if (mapGet(&d->map, STRING_VAL(name), &value)) {
            pop(); /* Instance */
            push(value);
            break;
          }
          runtimeError("Field '%s' not found in FrozenDict", name->chars);
          RETURN_RUNTIME_ERROR();
        }

        {
          ObjClass *cls = getClassOfValue(peek(0));
          if (cls->getattr) {
            push(STRING_VAL(READ_STRING()));
            if (!callCFunction(cls->getattr, 1)) {
              RETURN_RUNTIME_ERROR();
            }
            break;
          }
        }

        runtimeError(
            "%s values do not have have fields", getKindName(peek(0)));
        RETURN_RUNTIME_ERROR();
      }
      case OP_SET_FIELD: {
        Value value;

        if (IS_INSTANCE(peek(1))) {
          ObjInstance *instance;
          instance = AS_INSTANCE_UNSAFE(peek(1));
          mapSetStr(&instance->fields, READ_STRING(), peek(0));
          value = pop();
          pop();
          push(value);
          break;
        }

        if (IS_DICT(peek(1))) {
          ObjDict *d = AS_DICT_UNSAFE(peek(1));
          mapSet(&d->map, STRING_VAL(READ_STRING()), peek(0));
          value = pop();
          pop();
          push(value);
          break;
        }

        {
          ObjClass *cls = getClassOfValue(peek(1));
          if (cls->setattr) {
            value = pop();
            push(STRING_VAL(READ_STRING()));
            push(value);
            if (!callCFunction(cls->setattr, 2)) {
              RETURN_RUNTIME_ERROR();
            }
            break;
          }
        }

        runtimeError(
            "%s values do not have have fields", getKindName(peek(1)));
        RETURN_RUNTIME_ERROR();
        break;
      }
      case OP_IS: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesIs(a, b)));
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER: {
        ubool result = valueLessThan(peek(0), peek(1));
        pop();
        pop();
        push(BOOL_VAL(result));
        break;
      }
      case OP_LESS: {
        ubool result = valueLessThan(peek(1), peek(0));
        pop();
        pop();
        push(BOOL_VAL(result));
        break;
      }
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = pop().as.number;
          double a = pop().as.number;
          push(NUMBER_VAL(a + b));
        } else {
          INVOKE(vm.addString, 1);
        }
        break;
      }
      case OP_SUBTRACT:
        BINARY_OP(a - b, vm.subString);
        break;
      case OP_MULTIPLY:
        BINARY_OP(a * b, vm.mulString);
        break;
      case OP_DIVIDE:
        BINARY_OP(a / b, vm.divString);
        break;
      case OP_FLOOR_DIVIDE:
        BINARY_OP(floor(a / b), vm.floordivString);
        break;
      case OP_MODULO:
        BINARY_OP(mfmod(a, b), vm.modString);
        break;
      case OP_POWER:
        BINARY_OP(pow(a, b), vm.powString);
        break;
      case OP_SHIFT_LEFT:
        BINARY_BITWISE_OP("lshift", <<);
        break;
      case OP_SHIFT_RIGHT:
        BINARY_BITWISE_OP("rshift", >>);
        break;
      case OP_BITWISE_OR:
        BINARY_BITWISE_OP("bitwise or", |);
        break;
      case OP_BITWISE_AND:
        BINARY_BITWISE_OP("bitwise and", &);
        break;
      case OP_BITWISE_XOR:
        BINARY_BITWISE_OP("bitwise xor", ^);
        break;
      case OP_BITWISE_NOT: {
        u32 x;
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          RETURN_RUNTIME_ERROR();
        }
        x = asU32Bits(pop());
        push(NUMBER_VAL(~x));
        break;
      }
      case OP_IN: {
        if (IS_CLASS(peek(0))) {
          ObjClass *cls = AS_CLASS_UNSAFE(pop());
          push(BOOL_VAL(cls == getClassOfValue(pop())));
        } else {
          Value b = pop();
          Value a = pop();
          push(b);
          push(a);
          INVOKE(vm.containsString, 1);
        }
        break;
      }
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (IS_NUMBER(peek(0))) {
          push(NUMBER_VAL(-pop().as.number));
        } else {
          INVOKE(vm.negString, 0);
        }
        break;
      case OP_JUMP: {
        u16 offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        u16 offset = READ_SHORT();
        if (isFalsey(peek(0))) {
          frame->ip += offset;
        }
        break;
      }
      case OP_JUMP_IF_STOP_ITERATION: {
        u16 offset = READ_SHORT();
        if (IS_STOP_ITERATION(peek(0))) {
          frame->ip += offset;
        }
        break;
      }
      case OP_TRY_START: {
        u16 offset = READ_SHORT();
        TrySnapshot *snapshot;
        if (vm.trySnapshotsCount >= TRY_SNAPSHOTS_MAX) {
          panic("try snapshot overflow");
        }
        snapshot = &vm.trySnapshots[vm.trySnapshotsCount++];
        snapshot->frameCount = vm.frameCount;
        snapshot->ip = frame->ip + offset;
        snapshot->stackTop = vm.stackTop;
        if (frame != &vm.frames[vm.frameCount - 1]) {
          panic("internal vm frame error");
        }
        break;
      }
      case OP_TRY_END: {
        u16 offset = READ_SHORT();
        if (vm.trySnapshotsCount == 0) {
          panic("try snapshot underflow");
        }
        frame->ip += offset;
        vm.trySnapshotsCount--;
        break;
      }
      case OP_RAISE: {
        if (!IS_STRING(peek(0))) {
          panic("Only strings can be raised right now");
        }
        runtimeError("%s", peek(0).as.string->chars);
        RETURN_RUNTIME_ERROR();
      }
      case OP_GET_ITER: {
        Value iterable = peek(0);
        if (!isIterator(iterable)) {
          INVOKE(vm.iterString, 0);
        }
        break;
      }
      case OP_GET_NEXT: {
        push(peek(0));
        CALL(0);
        break;
      }
      case OP_LOOP: {
        u16 offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        i16 argCount = READ_BYTE();
        CALL(argCount);
        break;
      }
      case OP_INVOKE: {
        String *method = READ_STRING();
        i16 argCount = READ_BYTE();
        INVOKE(method, argCount);
        break;
      }
      case OP_SUPER_INVOKE: {
        String *method = READ_STRING();
        i16 argCount = READ_BYTE();
        ObjClass *superclass = AS_CLASS_UNSAFE(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          RETURN_RUNTIME_ERROR();
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjThunk *thunk = AS_THUNK_UNSAFE(READ_CONSTANT());
        ObjClosure *closure = newClosure(thunk, frame->closure->module);
        i16 i;
        push(CLOSURE_VAL(closure));
        for (i = 0; i < closure->upvalueCount; i++) {
          u8 isLocal = READ_BYTE();
          u8 index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] =
                captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == returnFrameCount) {
          vm.stackTop = frame->slots;
          push(result);
          if (vm.frameCount > 0) {
            frame = &vm.frames[vm.frameCount - 1];
          }

          return STATUS_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_IMPORT: {
        String *name = READ_STRING();
        if (!importModule(name)) {
          RETURN_RUNTIME_ERROR();
        }
        break;
      }
      case OP_NEW_LIST: {
        size_t i, length = READ_BYTE();
        ObjList *list = newList(length);
        Value *start = vm.stackTop - length;
        for (i = 0; i < length; i++) {
          list->buffer[i] = start[i];
        }
        *start = LIST_VAL(list);
        vm.stackTop = start + 1;
        break;
      }
      case OP_NEW_FROZEN_LIST: {
        size_t length = READ_BYTE();
        Value *start = vm.stackTop - length;
        ObjFrozenList *frozenList = copyFrozenList(start, length);
        *start = FROZEN_LIST_VAL(frozenList);
        vm.stackTop = start + 1;
        break;
      }
      case OP_NEW_DICT: {
        size_t i, length = READ_BYTE();
        ObjDict *dict = newDict();
        Value *start = vm.stackTop - 2 * length;
        ubool gcPause;
        LOCAL_GC_PAUSE(gcPause);
        for (i = 0; i < 2 * length; i += 2) {
          mapSet(&dict->map, start[i], start[i + 1]);
        }
        LOCAL_GC_UNPAUSE(gcPause);
        vm.stackTop = start;
        push(DICT_VAL(dict));
        break;
      }
      case OP_NEW_FROZEN_DICT: {
        size_t i, length = READ_BYTE();
        ObjFrozenDict *fdict;
        Map map;
        Value *start = vm.stackTop - 2 * length;
        initMap(&map);
        for (i = 0; i < 2 * length; i += 2) {
          mapSet(&map, start[i], start[i + 1]);
        }
        fdict = newFrozenDict(&map);
        vm.stackTop = start;
        push(FROZEN_DICT_VAL(fdict));
        freeMap(&map);
        break;
      }
      case OP_CLASS:
        push(CLASS_VAL(newClass(READ_STRING())));
        break;
      case OP_INHERIT: {
        Value superclass;
        ObjClass *subclass;
        superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeError("Superclass must be a class");
          RETURN_RUNTIME_ERROR();
        }

        subclass = AS_CLASS_UNSAFE(peek(0));
        mapAddAll(&AS_CLASS_UNSAFE(superclass)->methods, &subclass->methods);
        pop(); /* subclass */
        break;
      }
      case OP_METHOD:
        defineMethod(READ_STRING());
        break;
      case OP_STATIC_METHOD:
        defineStaticMethod(READ_STRING());
        break;
    }
  }
#undef BINARY_BITWISE_OP
#undef BINARY_OP
#undef CALL
#undef INVOKE
#undef RETURN_RUNTIME_ERROR
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_BYTE
}

/* Runs true on success, false otherwise */
Status interpret(const char *source, ObjModule *module) {
  ObjClosure *closure;
  ObjThunk *thunk;

  if (!parse(source, module->klass->name, &thunk)) {
    return STATUS_ERROR;
  }

  push(THUNK_VAL(thunk));
  closure = newClosure(thunk, module);
  pop();
  push(CLOSURE_VAL(closure));

  return callClosure(closure, 0);
}

ubool valueIsCString(Value value, const char *string) {
  return IS_STRING(value) && strcmp(value.as.string->chars, string) == 0;
}

void listAppend(ObjList *list, Value value) {
  if (list->capacity < list->length + 1) {
    size_t oldCapacity = list->capacity;
    list->capacity = GROW_CAPACITY(list->capacity);
    list->buffer = GROW_ARRAY(
        Value, list->buffer, oldCapacity, list->capacity);
  }
  list->buffer[list->length++] = value;
}

static Status callClass(ObjClass *klass, i16 argCount, ubool consummate) {
  Value initializer;

  if (klass->instantiate) {
    return callCFunction(klass->instantiate, argCount);
  } else if (klass->isBuiltinClass) {
    /* builtin class */
    runtimeError("Builtin class %s does not support being called",
                 klass->name->chars);
    return STATUS_ERROR;
  } else if (klass->descriptor) {
    /* native class */
    runtimeError("Native class %s does not support being called",
                 klass->name->chars);
    return STATUS_ERROR;
  } else if (klass->isModuleClass) {
    /* module */
    runtimeError("Module classes cannot be instantiated");
    return STATUS_ERROR;
  } else {
    /* normal classes */
    vm.stackTop[-argCount - 1] = INSTANCE_VAL(newInstance(klass));
    if (mapGetStr(&klass->methods, vm.initString, &initializer)) {
      if (!setupCallClosure(AS_CLOSURE_UNSAFE(initializer), argCount)) {
        return STATUS_ERROR;
      }
      if (consummate) {
        return run();
      }
      return STATUS_OK;
    } else if (argCount != 0) {
      runtimeError("Expected 0 arguments but got %d", argCount);
      return STATUS_ERROR;
    }
    return STATUS_OK;
  }
}

Status callClosure(ObjClosure *closure, i16 argCount) {
  return setupCallClosure(closure, argCount) && run();
}

/*
 * Helper function for callFunction and callMethod.
 * The stack is expected to look like what callFunction specifies,
 * but the callable is passed explicitly as a C argument
 * rather than being passed implicitly on the stack.
 *
 * This means that the slot on the stack where the callable is expected
 * to be is only used to place the return value, but ignored otherwise.
 *
 * Still, if the passed callable is a method, the receiver slot must
 * be set to the proper receiver value before this function is called.
 *
 * The consummate flag indicates whether the call should be taken to completion.
 * That is to say, when the time comes we avoid calling 'run' and instead just
 * set up the stack frame so that code in 'run' may resume from where it was
 * and it would still work.
 */
static Status callFunctionOrMethod(Value callable, i16 argCount, ubool consummate) {
  if (IS_CFUNCTION(callable)) {
    CFunction *cfunc = callable.as.cfunction;
    return callCFunction(cfunc, argCount);
  } else if (IS_OBJ(callable)) {
    switch (OBJ_TYPE(callable)) {
      case OBJ_CLASS:
        return callClass(AS_CLASS_UNSAFE(callable), argCount, consummate);
      case OBJ_CLOSURE:
        return callClosure(AS_CLOSURE_UNSAFE(callable), argCount);
      case OBJ_NATIVE: {
        ObjNative *n = AS_NATIVE_UNSAFE(callable);
        if (n->descriptor->klass->call) {
          return callCFunction(n->descriptor->klass->call, argCount);
        }
        break;
      }
      default:
        break; /* Non-callable object type */
    }
  }
  runtimeError(
      "Can only call functions and classes but got %s", getKindName(callable));
  return STATUS_ERROR;
}

static Status callMethodHelper(String *name, i16 argCount, ubool consummate) {
  ObjClass *klass;
  Value receiver = peek(argCount);
  Value method;

  klass = getClassOfValue(receiver);
  if (klass == NULL) {
    panic("Could not get class for kind %s", getKindName(receiver));
  }

  if (klass == vm.classClass) {
    /* For classes, we invoke static methods */
    ObjClass *cls;
    if (!IS_CLASS(receiver)) {
      panic("Class instance is not a Class (%s)", getKindName(receiver));
    }
    cls = AS_CLASS_UNSAFE(receiver);
    if (!mapGetStr(&cls->staticMethods, name, &method)) {
      runtimeError(
          "Static method '%s' not found in '%s'",
          name->chars,
          cls->name->chars);
      return STATUS_ERROR;
    }
    return callFunctionOrMethod(method, argCount, consummate);
  }

  if (!mapGetStr(&klass->methods, name, &method)) {
    runtimeError(
        "Method '%s' not found in '%s'",
        name->chars,
        klass->name->chars);
    return STATUS_ERROR;
  }
  return callFunctionOrMethod(method, argCount, consummate);
}

Status callFunction(i16 argCount) {
  return callFunctionOrMethod(peek(argCount), argCount, UTRUE);
}

Status callMethod(String *name, i16 argCount) {
  return callMethodHelper(name, argCount, UTRUE);
}
