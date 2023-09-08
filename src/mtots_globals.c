#include "mtots_globals.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_vm.h"

static ubool implClock(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(clock() / (double)CLOCKS_PER_SEC);
  return UTRUE;
}

static CFunction cfunctionClock = {implClock, "clock"};

static ubool implExit(i16 argCount, Value *args, Value *out) {
  int exitCode = 0;
  if (argCount > 0) {
    exitCode = asNumber(args[0]);
  }
  exit(exitCode);
  return UTRUE;
}

static CFunction cfuncExit = {implExit, "exit", 0, 1};

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

static CFunction cfuncGetErrorString = {implGetErrorString, "getErrorString"};

static ubool implLen(i16 argCount, Value *args, Value *out) {
  Value recv = args[0];
  size_t len;
  if (!valueLen(recv, &len)) {
    return UFALSE;
  }
  *out = NUMBER_VAL(len);
  return UTRUE;
}

static CFunction cfuncLen = {implLen, "len", 1};

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
    total += asNumber(item);
  }
  pop(); /* iterator */
  *out = NUMBER_VAL(total);
  return UTRUE;
}

static CFunction cfuncSum = {implSum, "sum", 1};

static ubool implHex(i16 argCount, Value *args, Value *out) {
  double rawValue = asNumber(args[0]);
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

static CFunction funcHex = {implHex, "hex", 1};

static ubool implBin(i16 argCount, Value *args, Value *out) {
  double rawValue = asNumber(args[0]);
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

static CFunction funcBin = {implBin, "bin", 1};

static ubool implRound(i16 argCount, Value *args, Value *out) {
  double number = asNumber(args[0]);
  *out = NUMBER_VAL((long)(number + 0.5));
  return UTRUE;
}

static CFunction cfuncRound = {implRound, "round", 0, 1};

static ubool implType(i16 argCount, Value *args, Value *out) {
  *out = CLASS_VAL(getClassOfValue(args[0]));
  return UTRUE;
}

static CFunction cfuncType = {implType, "type", 1};

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

static CFunction cfunctionRepr = {implRepr, "repr", 1};

static ubool implStr(i16 argCount, Value *args, Value *out) {
  if (IS_STRING(*args)) {
    *out = *args;
    return UTRUE;
  }
  return implRepr(argCount, args, out);
}

static CFunction cfunctionStr = {implStr, "str", 1};

static ubool implChr(i16 argCount, Value *args, Value *out) {
  char c;
  if (!IS_NUMBER(args[0])) {
    runtimeError("chr() requires a number but got %s",
                 getKindName(args[0]));
    return UFALSE;
  }
  c = (char)asI32(args[0]);
  *out = STRING_VAL(internString(&c, 1));
  return UTRUE;
}

static CFunction cfunctionChr = {implChr, "chr", 1};

static ubool implOrd(i16 argCount, Value *args, Value *out) {
  String *str;
  str = asString(args[0]);
  if (str->byteLength != 1) {
    runtimeError(
        "ord() requires a string of length 1 but got a string of length %lu",
        (long)str->byteLength);
    return UFALSE;
  }
  *out = NUMBER_VAL((u8)str->chars[0]);
  return UTRUE;
}

static CFunction cfunctionOrd = {implOrd, "ord", 1};

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

static CFunction cfunctionMin = {implMin, "min", 1, MAX_ARG_COUNT};

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

static CFunction cfunctionMax = {implMax, "max", 1, MAX_ARG_COUNT};

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

static CFunction cfunctionSorted = {implSorted, "sorted", 1, 2};

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
  pop();        /* iterator */
  *out = pop(); /* dict */
  return UTRUE;
}

static CFunction cfunctionSet = {implSet, "Set", 1};

static ubool implTuple(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList = copyFrozenList(args, argCount);
  *out = FROZEN_LIST_VAL(frozenList);
  return UTRUE;
}

static CFunction cfunctionTuple = {implTuple, "Tuple", 1, 4};

static ubool implPrint(i16 argCount, Value *args, Value *out) {
  Value strVal;
  if (!implStr(argCount, args, &strVal)) {
    return UFALSE;
  }
  oprintln("%s", asString(strVal)->chars);
  return UTRUE;
}

static CFunction cfunctionPrint = {implPrint, "print", 1};

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
      stop = asNumber(args[0]);
      break;
    case 2:
      start = asNumber(args[0]);
      stop = asNumber(args[1]);
      break;
    case 3:
      start = asNumber(args[0]);
      stop = asNumber(args[1]);
      step = asNumber(args[2]);
      break;
    default:
      panic("Invalid argc to range() (%d)", argCount);
      return UFALSE;
  }
  *out = OBJ_VAL_EXPLICIT((Obj *)newRange(start, stop, step));
  return UTRUE;
}

static CFunction cfunctionRange = {implRange, "range", 1, 3};

static ubool implFloat(i16 argCount, Value *args, Value *out) {
  Value arg = args[0];
  if (IS_NUMBER(arg)) {
    *out = arg;
    return UTRUE;
  }
  if (IS_STRING(arg)) {
    String *str = (String *)arg.as.obj;
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

static CFunction funcFloat = {implFloat, "float", 1};

static ubool implInt(i16 argCount, Value *args, Value *out) {
  Value arg = args[0];
  if (IS_NUMBER(arg)) {
    *out = NUMBER_VAL(floor(asNumber(arg)));
    return UTRUE;
  }
  if (IS_STRING(arg)) {
    String *str = (String *)arg.as.obj;
    i32 base = argCount > 1 ? asI32(args[1]) : 10;
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

static CFunction funcInt = {implInt, "int", 1, 2};

static ubool implIsClose(i16 argc, Value *args, Value *out) {
  Value a = args[0];
  Value b = args[1];
  double relTol = argc > 2 ? asNumber(args[2]) : DEFAULT_RELATIVE_TOLERANCE;
  double absTol = argc > 3 ? asNumber(args[3]) : DEFAULT_ABSOLUTE_TOLERANCE;
  if (IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
    *out = BOOL_VAL(doubleIsCloseEx(asNumber(a), asNumber(b), relTol, absTol));
    return UTRUE;
  }
  runtimeError(
      "Expectecd two Numbers, but got %s and %s",
      getKindName(a), getKindName(b));
  return UFALSE;
}

static CFunction funcIsClose = {implIsClose, "isClose", 2, 4};

static ubool implSin(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(sin(asNumber(args[0])));
  return UTRUE;
}

static CFunction funcSin = {implSin, "sin", 1};

static ubool implCos(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(cos(asNumber(args[0])));
  return UTRUE;
}

static CFunction funcCos = {implCos, "cos", 1};

static ubool implTan(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(tan(asNumber(args[0])));
  return UTRUE;
}

static CFunction funcTan = {implTan, "tan", 1};

static ubool implAbs(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  *out = NUMBER_VAL(value < 0 ? -value : value);
  return UTRUE;
}

static CFunction funcAbs = {implAbs, "abs", 1};

static ubool implLog(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  if (!(value > 0)) { /* The '!' is to account for NaN */
    runtimeError("Argument to log() must be positive but got %f", value);
    return UFALSE;
  }
  *out = NUMBER_VAL(log(value));
  return UTRUE;
}

static CFunction funcLog = {implLog, "log", 1};

static ubool implFlog2(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
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

static CFunction funcFlog2 = {implFlog2, "flog2", 1};

static ubool implIsInstance(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(asClass(args[1]) == getClassOfValue(args[0]));
  return UTRUE;
}

static CFunction funcIsInstance = {implIsInstance, "isinstance", 2};

static ubool implSort(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[0]);
  ObjList *keys =
      argCount < 2      ? NULL
      : IS_NIL(args[1]) ? NULL
                        : asList(args[1]);
  sortList(list, keys);
  return UTRUE;
}

static CFunction funcSort = {implSort, "__sort__", 1, 2};

void defineDefaultGlobals(void) {
  NativeObjectDescriptor *descriptors[] = {
      &descriptorStringBuilder,
      NULL,
  },
                         **descriptor;
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
      &funcIsClose,
      &funcSin,
      &funcCos,
      &funcTan,
      &funcAbs,
      &funcLog,
      &funcFlog2,
      &funcIsInstance,
      &funcSort,
      NULL,
  },
            **function;

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
      NULL,
  },
           ***builtinClass;

  defineGlobal("PI", NUMBER_VAL(PI));
  defineGlobal("TAU", NUMBER_VAL(TAU));

#ifdef NAN
  defineGlobal("NAN", NUMBER_VAL(NAN));
#else
  defineGlobal("NAN", NUMBER_VAL(0.0 / 0.0));
#endif

#ifdef INFINITY
  defineGlobal("INFINITY", NUMBER_VAL(INFINITY));
#else
  defineGlobal("INFINITY", NUMBER_VAL(1.0 / 0.0));
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
