#include "mtots_globals.h"
#include "mtots_vm.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>


static ubool implClock(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
  return UTRUE;
}

static CFunction cfunctionClock = { implClock, "clock", 0 };

static ubool implExit(i16 argCount, Value *args, Value *out) {
  int exitCode = 0;
  if (argCount > 0) {
    exitCode = AS_NUMBER(args[0]);
  }
  exit(exitCode);
  return UTRUE;
}

static CFunction cfuncExit = { implExit, "exit", 0, 1, argsNumbers };

static ubool implGetErrorString(i16 argCount, Value *args, Value *out) {
  const char *errorString = getSavedErrorString();
  if (errorString == NULL) {
    runtimeError("No error string found");
    return UFALSE;
  }
  *out = STRING_VAL(internCString(errorString));
  clearSavedErrorString();
  return UTRUE;
}

static CFunction cfuncGetErrorString = { implGetErrorString, "getErrorString", 0 };

static ubool implLen(i16 argCount, Value *args, Value *out) {
  Value recv = args[0];
  size_t len;
  if (!valueLen(recv, &len)) {
    return UFALSE;
  }
  *out = NUMBER_VAL(len);
  return UTRUE;
}

static CFunction cfuncLen = { implLen, "len", 1 };

static ubool implSum(i16 argCount, Value *args, Value *out) {
  Value iterable = args[0];
  Value iterator, item;
  double total = 0;
  if (!valueFastIter(iterable, &iterator)) {
    return UFALSE;
  }
  push(iterator);
    for (;;) {
    if (!valueFastIterNext(&iterator, &item)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(item)) {
      break;
    }
    if (!IS_NUMBER(item)) {
      runtimeError("Expected number but got %s", getKindName(item));
      return UFALSE;
    }
    total += AS_NUMBER(item);
  }
  pop(); /* iterator */
  *out = NUMBER_VAL(total);
  return UTRUE;
}

static CFunction cfuncSum = { implSum, "sum", 1 };

static ubool implHex(i16 argCount, Value *args, Value *out) {
  double rawValue = AS_NUMBER(args[0]);
  size_t start, end, value;
  Buffer buf;

  initBuffer(&buf);
  if (rawValue < 0) {
    bputchar(&buf, '-');
    rawValue *= -1;
  }
  value = (size_t)rawValue;
  bputstr(&buf, "0x");
  if (value == 0) {
    bputchar(&buf, '0');
  } else {
    start = buf.length;
    while (value) {
      size_t digit = value & 0xF;
      if (digit < 10) {
        bputchar(&buf, '0' + digit);
      } else {
        bputchar(&buf, 'A' + (digit - 10));
      }
      value /= 0x10;
    }
    end = buf.length;
    while (start + 1 < end) {
      u8 tmp = buf.data[start];
      buf.data[start] = buf.data[end - 1];
      buf.data[end - 1] = tmp;
      start++;
      end--;
    }
  }

  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction funcHex = { implHex, "hex", 1, 0, argsNumbers };

static ubool implBin(i16 argCount, Value *args, Value *out) {
  double rawValue = AS_NUMBER(args[0]);
  size_t start, end, value;
  Buffer buf;

  initBuffer(&buf);
  if (rawValue < 0) {
    bputchar(&buf, '-');
    rawValue *= -1;
  }
  value = (size_t)rawValue;
  bputstr(&buf, "0b");
  if (value == 0) {
    bputchar(&buf, '0');
  } else {
    start = buf.length;
    while (value) {
      bputchar(&buf, (value & 1) ? '1' : '0');
      value /= 2;
    }
    end = buf.length;
    while (start + 1 < end) {
      u8 tmp = buf.data[start];
      buf.data[start] = buf.data[end - 1];
      buf.data[end - 1] = tmp;
      start++;
      end--;
    }
  }

  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction funcBin = { implBin, "bin", 1, 0, argsNumbers };

static ubool implRound(i16 argCount, Value *args, Value *out) {
  double number = AS_NUMBER(args[0]);
  *out = NUMBER_VAL((long)(number + 0.5));
  return UTRUE;
}

static CFunction cfuncRound = { implRound, "round", 0, 1, argsNumbers };

static ubool implType(i16 argCount, Value *args, Value *out) {
  *out = CLASS_VAL(getClassOfValue(args[0]));
  return UTRUE;
}

static CFunction cfuncType = { implType, "type", 1};

static ubool implRepr(i16 argCount, Value *args, Value *out) {
  Buffer buf;
  initBuffer(&buf);
  if (!valueRepr(&buf, args[0])) {
    freeBuffer(&buf);
    return UFALSE;
  }
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction cfunctionRepr = { implRepr, "repr", 1 };

static ubool implStr(i16 argCount, Value *args, Value *out) {
  if (IS_STRING(*args)) {
    *out = *args;
    return UTRUE;
  }
  return implRepr(argCount, args, out);
}

static CFunction cfunctionStr = { implStr, "str", 1 };

static ubool implChr(i16 argCount, Value *args, Value *out) {
  char c;
  if (!IS_NUMBER(args[0])) {
    runtimeError("chr() requires a number but got %s",
      getKindName(args[0]));
    return UFALSE;
  }
  c = (char) (i32) AS_NUMBER(args[0]);
  *out = STRING_VAL(internString(&c, 1));
  return UTRUE;
}

static CFunction cfunctionChr = { implChr, "chr", 1 };

static ubool implOrd(i16 argCount, Value *args, Value *out) {
  String *str;
  if (!IS_STRING(args[0])) {
    runtimeError("ord() requires a string but got %s",
      getKindName(args[0]));
    return UFALSE;
  }
  str = AS_STRING(args[0]);
  if (str->byteLength != 1) {
    runtimeError(
      "ord() requires a string of length 1 but got a string of length %lu",
      (long) str->byteLength);
    return UFALSE;
  }
  *out = NUMBER_VAL((u8)str->chars[0]);
  return UTRUE;
}

static CFunction cfunctionOrd = { implOrd, "ord", 1 };

static ubool implMin(i16 argCount, Value *args, Value *out) {
  Value best = args[0];
  i16 i;
  for (i = 1; i < argCount; i++) {
    if (IS_NIL(args[i])) {
      break;
    }
    if (valueLessThan(args[i], best)) {
      best = args[i];
    }
  }
  *out = best;
  return UTRUE;
}

static CFunction cfunctionMin = { implMin, "min", 1, MAX_ARG_COUNT };

static ubool implMax(i16 argCount, Value *args, Value *out) {
  Value best = args[0];
  i16 i;
  for (i = 1; i < argCount; i++) {
    if (IS_NIL(args[i])) {
      break;
    }
    if (valueLessThan(best, args[i])) {
      best = args[i];
    }
  }
  *out = best;
  return UTRUE;
}

static CFunction cfunctionMax = { implMax, "max", 1, MAX_ARG_COUNT };

static ubool implSorted(i16 argCount, Value *args, Value *out) {
  ObjList *list;
  if (!newListFromIterable(args[0], &list)) {
    return UFALSE;
  }
  push(LIST_VAL(list));
  if (!sortListWithKeyFunc(list, argCount > 1 ? args[1] : NIL_VAL())) {
    return UFALSE;
  }
  *out = pop(); /* list */
  return UTRUE;
}

static CFunction cfunctionSorted = { implSorted, "sorted", 1, 2 };

static ubool implSet(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = newDict();
  Value iterator;
  push(DICT_VAL(dict));
  if (!valueFastIter(args[0], &iterator)) {
    return UFALSE;
  }
  push(iterator);
  for (;;) {
    Value key;
    if (!valueFastIterNext(&iterator, &key)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(key)) {
      break;
    }
    push(key);
    mapSet(&dict->map, key, NIL_VAL());
    pop(); /* key */
  }
  pop(); /* iterator */
  *out = pop(); /* dict */
  return UTRUE;
}

static CFunction cfunctionSet = { implSet, "Set", 1 };

static ubool implTuple(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList = copyFrozenList(args, argCount);
  *out = FROZEN_LIST_VAL(frozenList);
  return UTRUE;
}

static CFunction cfunctionTuple = { implTuple, "Tuple", 1, 4 };

static ubool implPrint(i16 argCount, Value *args, Value *out) {
  Value strVal;
  if (!implStr(argCount, args, &strVal)) {
    return UFALSE;
  }
  if (!IS_STRING(strVal)) {
    runtimeError("'str' returned a non-string value");
    return UFALSE;
  }
  oprintln("%s", AS_STRING(strVal)->chars);
  return UTRUE;
}

static CFunction cfunctionPrint = { implPrint, "print", 1 };

static ubool implRange(i16 argCount, Value *args, Value *out) {
  double start = 0, stop, step = 1;
  i32 i = 0;
  for (i = 0; i < argCount; i++) {
    if (!IS_NUMBER(args[i])) {
      panic(
        "range() requires number arguments but got %s for argument %d",
        getKindName(args[i]), i);
    }
  }
  switch (argCount) {
    case 1:
      stop = AS_NUMBER(args[0]);
      break;
    case 2:
      start = AS_NUMBER(args[0]);
      stop = AS_NUMBER(args[1]);
      break;
    case 3:
      start = AS_NUMBER(args[0]);
      stop = AS_NUMBER(args[1]);
      step = AS_NUMBER(args[2]);
      break;
    default:
      panic("Invalid argc to range() (%d)", argCount);
      return UFALSE;
  }
  if (doubleIsI32(start) && doubleIsI32(stop) && doubleIsI32(step)) {
    FastRange fastRange;
    fastRange.start = (i32)start;
    fastRange.stop = (i32)stop;
    fastRange.step = (i32)step;
    *out = FAST_RANGE_VAL(fastRange);
  } else {
    *out = OBJ_VAL_EXPLICIT((Obj*)newRange(start, stop, step));
  }
  return UTRUE;
}

static CFunction cfunctionRange = { implRange, "range", 1, 3 };

static ubool implFloat(i16 argCount, Value *args, Value *out) {
  Value arg = args[0];
  if (IS_NUMBER(arg)) {
    *out = arg;
    return UTRUE;
  }
  if (IS_STRING(arg)) {
    String *str = AS_STRING(arg);
    const char *ptr = str->chars;
    ubool decimalPoint = UFALSE;
    if (*ptr == '-' || *ptr == '+') {
      ptr++;
    }
    if (*ptr == '.') {
      ptr++;
      decimalPoint = UTRUE;
    }
    if ('0' <= *ptr && *ptr <= '9') {
      ptr++;
      while ('0' <= *ptr && *ptr <= '9') {
        ptr++;
      }
      if (!decimalPoint && *ptr == '.') {
        decimalPoint = UTRUE;
        ptr++;
        while ('0' <= *ptr && *ptr <= '9') {
          ptr++;
        }
      }
      if (*ptr == 'e' || *ptr == 'E') {
        ptr++;
        if (*ptr == '-' || *ptr == '+') {
          ptr++;
        }
        while ('0' <= *ptr && *ptr <= '9') {
          ptr++;
        }
      }
      if (*ptr == '\0') {
        *out = NUMBER_VAL(strtod(str->chars, NULL));
        return UTRUE;
      }
    }
    runtimeError("Could not convert string to float: %s", str->chars);
    return UFALSE;
  }
  runtimeError("%s is not convertible to float", getKindName(arg));
  return UFALSE;
}

static CFunction funcFloat = { implFloat, "float", 1 };

static ubool implInt(i16 argCount, Value *args, Value *out) {
  Value arg = args[0];
  if (IS_NUMBER(arg)) {
    *out = NUMBER_VAL(floor(AS_NUMBER(arg)));
    return UTRUE;
  }
  if (IS_STRING(arg)) {
    String *str = AS_STRING(arg);
    i32 base = argCount > 1 ? AS_I32(args[1]) : 10;
    const char *ptr = str->chars;
    double value = 0, sign = 1;
    if (base < 2) {
      runtimeError("int(): unsupported base %d", (int)base);
      return UFALSE;
    }
    if (*ptr == '-' || *ptr == '+') {
      sign = *ptr == '-' ? -1 : 1;
      ptr++;
    }
    if (*ptr == '\0') {
      runtimeError("int(): Expected digit, but got end of string");
      return UFALSE;
    }
    while (('0' <= *ptr && *ptr <= '9') ||
        ('A' <= *ptr && *ptr <= 'Z') ||
        ('a' <= *ptr && *ptr <= 'z')) {
      char ch = *ptr++;
      i32 digit = -1;
      if ('0' <= ch && ch <= '9') {
        digit = ch - '0';
      } else if ('A' <= ch && ch <= 'Z') {
        digit = 10 + (ch - 'A');
      } else if ('a' <= ch && ch <= 'z') {
        digit = 10 + (ch - 'a');
      } else {
        panic("assertion failed (int(), ch='%c')", ch);
      }
      if (digit >= base) {
        runtimeError(
          "int(): digit value is too big for base (digit=%d, base=%d)",
          (int)digit, (int)base);
        return UFALSE;
      }
      value = value * base + digit;
    }
    if (*ptr != '\0') {
      runtimeError("int(): Expected digit but got '%c'", *ptr);
      return UFALSE;
    }
    *out = NUMBER_VAL(value * sign);
    return UTRUE;
  }
  runtimeError("%s is not convertible to int", getKindName(arg));
  return UFALSE;
}

static TypePattern argsInt[] = {
  { TYPE_PATTERN_ANY },
  { TYPE_PATTERN_NUMBER },
};

static CFunction funcInt = { implInt, "int", 1, 2, argsInt };

static ubool implSin(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(sin(AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcSin = { implSin, "sin", 1, 0, argsNumbers };

static ubool implCos(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(cos(AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcCos = { implCos, "cos", 1, 0, argsNumbers };

static ubool implTan(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(tan(AS_NUMBER(args[0])));
  return UTRUE;
}

static CFunction funcTan = { implTan, "tan", 1, 0, argsNumbers };

static ubool implAbs(i16 argCount, Value *args, Value *out) {
  double value = AS_NUMBER(args[0]);
  *out = NUMBER_VAL(value < 0 ? -value : value);
  return UTRUE;
}

static CFunction funcAbs = { implAbs, "abs", 1, 0, argsNumbers };

static ubool implLog(i16 argCount, Value *args, Value *out) {
  double value = AS_NUMBER(args[0]);
  if (!(value > 0)) { /* The '!' is to account for NaN */
    runtimeError("Argument to log() must be positive but got %f", value);
    return UFALSE;
  }
  *out = NUMBER_VAL(log(value));
  return UTRUE;
}

static CFunction funcLog = { implLog, "log", 1, 0, argsNumbers };

static ubool implFlog2(i16 argCount, Value *args, Value *out) {
  double value = AS_NUMBER(args[0]);
  i32 ret = 0;
  if (!(value > 0)) { /* The '!' is to account for NaN */
    runtimeError("Argument to flog2() must be positive but got %f", value);
    return UFALSE;
  }
  while (value > 1) {
    value /= 2;
    ret++;
  }
  *out = NUMBER_VAL(ret);
  return UTRUE;
}

static CFunction funcFlog2 = { implFlog2, "flog2", 1, 0, argsNumbers };

static ubool implIsInstance(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(AS_CLASS(args[1]) == getClassOfValue(args[0]));
  return UTRUE;
}

static TypePattern argsIsInstance[] = {
  { TYPE_PATTERN_ANY },
  { TYPE_PATTERN_CLASS },
};

static CFunction funcIsInstance = {
  implIsInstance, "isinstance", sizeof(argsIsInstance)/sizeof(TypePattern), 0, argsIsInstance
};

static ubool implSort(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[0]);
  ObjList *keys =
    argCount < 2 ?
      NULL :
      IS_NIL(args[1]) ?
        NULL :
        AS_LIST(args[1]);
  sortList(list, keys);
  return UTRUE;
}

static TypePattern argsSort[] = {
  { TYPE_PATTERN_LIST },
  { TYPE_PATTERN_LIST_OR_NIL },
};

static CFunction funcSort = { implSort, "__sort__", 1, 2, argsSort };

static ubool implV(i16 argc, Value *args, Value *out) {
  double x = AS_NUMBER(args[0]);
  double y = AS_NUMBER(args[1]);
  double z = argc > 2 ? AS_NUMBER(args[2]) : 0;
  *out = VECTOR_VAL(newVector(x, y, z));
  return UTRUE;
}

static CFunction funcV = { implV, "V", 2, 3, argsNumbers };

static ubool implM(i16 argc, Value *args, Value *out) {
  ObjMatrix *matrix = newIdentityMatrix();
  ObjList *rows[4];
  size_t i;
  rows[0] = AS_LIST(args[0]);
  rows[1] = AS_LIST(args[1]);
  rows[2] = argc > 2 && !IS_NIL(args[2]) ? AS_LIST(args[2]) : NULL;
  rows[3] = argc > 3 && !IS_NIL(args[3]) ? AS_LIST(args[3]) : NULL;
  for (i = 0; i < 4; i++) {
    size_t j;
    ObjList *list = rows[i];
    if (!list) {
      continue;
    }
    for (j = 0; j < 4 && j < list->length; j++) {
      matrix->handle.row[i][j] = AS_NUMBER(list->buffer[j]);
    }
  }
  *out = MATRIX_VAL(matrix);
  return UTRUE;
}

static TypePattern argsM[] = {
  { TYPE_PATTERN_LIST_NUMBER },
  { TYPE_PATTERN_LIST_NUMBER },
  { TYPE_PATTERN_LIST_NUMBER_OR_NIL },
  { TYPE_PATTERN_LIST_NUMBER_OR_NIL },
};

static CFunction funcM = { implM, "M", 2, 4, argsM };

void defineDefaultGlobals() {
  NativeObjectDescriptor *descriptors[] = {
    &descriptorStringBuilder,
    &descriptorMatrix,
    &descriptorRect,
    NULL,
  }, **descriptor;
  CFunction *functions[] = {
    &cfunctionClock,
    &cfuncExit,
    &cfuncGetErrorString,
    &cfuncLen,
    &cfuncSum,
    &funcHex,
    &funcBin,
    &cfuncRound,
    &cfuncType,
    &cfunctionRepr,
    &cfunctionStr,
    &cfunctionChr,
    &cfunctionOrd,
    &cfunctionMin,
    &cfunctionMax,
    &cfunctionSorted,
    &cfunctionSet,
    &cfunctionTuple,
    &cfunctionPrint,
    &cfunctionRange,
    &funcFloat,
    &funcInt,
    &funcSin,
    &funcCos,
    &funcTan,
    &funcAbs,
    &funcLog,
    &funcFlog2,
    &funcIsInstance,
    &funcSort,
    &funcV,
    &funcM,
    NULL,
  }, **function;

  /* I hate to be 3-star programmer here, but C initializer lists require
   * compile time constants, and the extra level of indirection
   * helps with that */
  ObjClass **builtinClasses[] = {
    &vm.sentinelClass,
    &vm.nilClass,
    &vm.boolClass,
    &vm.numberClass,
    &vm.stringClass,
    &vm.bufferClass,
    &vm.listClass,
    &vm.frozenListClass,
    &vm.dictClass,
    &vm.frozenDictClass,
    &vm.functionClass,
    &vm.classClass,
    &vm.colorClass,
    &vm.vectorClass,
    NULL,
  }, ***builtinClass;

  defineGlobal("PI", NUMBER_VAL(PI));
  defineGlobal("TAU", NUMBER_VAL(TAU));

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  {
    double one = 1.0;
    double zero = one - one;
    defineGlobal("NAN", NUMBER_VAL(zero / zero));
    defineGlobal("INFINITY", NUMBER_VAL(one / zero));
  }
#else
  defineGlobal("NAN", NUMBER_VAL(0.0/0.0));
  defineGlobal("INFINITY", NUMBER_VAL(1.0/0.0));
#endif

  defineGlobal("StopIteration", STOP_ITERATION_VAL());

  for (function = functions; *function; function++) {
    defineGlobal((*function)->name, CFUNCTION_VAL(*function));
  }
  defineGlobal("__print", CFUNCTION_VAL(&cfunctionPrint));

  for (builtinClass = builtinClasses; *builtinClass; builtinClass++) {
    mapSetStr(&vm.globals, (**builtinClass)->name, CLASS_VAL(**builtinClass));
  }

  for (descriptor = descriptors; *descriptor; descriptor++) {
    NativeObjectDescriptor *d = *descriptor;
    mapSetStr(&vm.globals, d->klass->name, CLASS_VAL(d->klass));
  }
}
