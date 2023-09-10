#include "mtots_globals.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_vm.h"

static Status implClock(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(clock() / (double)CLOCKS_PER_SEC);
  return STATUS_OK;
}

static CFunction cfunctionClock = {implClock, "clock"};

static Status implExit(i16 argCount, Value *args, Value *out) {
  int exitCode = 0;
  if (argCount > 0) {
    exitCode = asNumber(args[0]);
  }
  exit(exitCode);
}

static CFunction cfuncExit = {implExit, "exit", 0, 1};

static Status implGetErrorString(i16 argCount, Value *args, Value *out) {
  const char *errorString = getSavedErrorString();
  if (errorString == NULL) {
    runtimeError("No error string found");
    return STATUS_ERROR;
  }
  *out = STRING_VAL(internCString(errorString));
  clearSavedErrorString();
  return STATUS_OK;
}

static CFunction cfuncGetErrorString = {implGetErrorString, "getErrorString"};

static Status implLen(i16 argCount, Value *args, Value *out) {
  Value recv = args[0];
  size_t len;
  if (!valueLen(recv, &len)) {
    return STATUS_ERROR;
  }
  *out = NUMBER_VAL(len);
  return STATUS_OK;
}

static CFunction cfuncLen = {implLen, "len", 1};

static Status implSum(i16 argCount, Value *args, Value *out) {
  Value iterable = args[0];
  Value iterator, item;
  double total = 0;
  if (!valueFastIter(iterable, &iterator)) {
    return STATUS_ERROR;
  }
  push(iterator);
  for (;;) {
    if (!valueFastIterNext(&iterator, &item)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(item)) {
      break;
    }
    if (!isNumber(item)) {
      runtimeError("Expected number but got %s", getKindName(item));
      return STATUS_ERROR;
    }
    total += asNumber(item);
  }
  pop(); /* iterator */
  *out = NUMBER_VAL(total);
  return STATUS_OK;
}

static CFunction cfuncSum = {implSum, "sum", 1};

static Status implHex(i16 argCount, Value *args, Value *out) {
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
  return STATUS_OK;
}

static CFunction funcHex = {implHex, "hex", 1};

static Status implBin(i16 argCount, Value *args, Value *out) {
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
  return STATUS_OK;
}

static CFunction funcBin = {implBin, "bin", 1};

static Status implRound(i16 argCount, Value *args, Value *out) {
  double number = asNumber(args[0]);
  *out = NUMBER_VAL((long)(number + 0.5));
  return STATUS_OK;
}

static CFunction cfuncRound = {implRound, "round", 0, 1};

static Status implType(i16 argCount, Value *args, Value *out) {
  *out = CLASS_VAL(getClassOfValue(args[0]));
  return STATUS_OK;
}

static CFunction cfuncType = {implType, "type", 1};

static Status implRepr(i16 argCount, Value *args, Value *out) {
  Buffer buf;
  initBuffer(&buf);
  if (!valueRepr(&buf, args[0])) {
    freeBuffer(&buf);
    return STATUS_ERROR;
  }
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return STATUS_OK;
}

static CFunction cfunctionRepr = {implRepr, "repr", 1};

static Status implStr(i16 argCount, Value *args, Value *out) {
  if (isString(*args)) {
    *out = *args;
    return STATUS_OK;
  }
  return implRepr(argCount, args, out);
}

static CFunction cfunctionStr = {implStr, "str", 1};

static Status implChr(i16 argCount, Value *args, Value *out) {
  char c;
  if (!isNumber(args[0])) {
    runtimeError("chr() requires a number but got %s",
                 getKindName(args[0]));
    return STATUS_ERROR;
  }
  c = (char)asI32(args[0]);
  *out = STRING_VAL(internString(&c, 1));
  return STATUS_OK;
}

static CFunction cfunctionChr = {implChr, "chr", 1};

static Status implOrd(i16 argCount, Value *args, Value *out) {
  String *str;
  str = asString(args[0]);
  if (str->byteLength != 1) {
    runtimeError(
        "ord() requires a string of length 1 but got a string of length %lu",
        (long)str->byteLength);
    return STATUS_ERROR;
  }
  *out = NUMBER_VAL((u8)str->chars[0]);
  return STATUS_OK;
}

static CFunction cfunctionOrd = {implOrd, "ord", 1};

static Status implMin(i16 argCount, Value *args, Value *out) {
  Value best = args[0];
  i16 i;
  for (i = 1; i < argCount; i++) {
    if (isNil(args[i])) {
      break;
    }
    if (valueLessThan(args[i], best)) {
      best = args[i];
    }
  }
  *out = best;
  return STATUS_OK;
}

static CFunction cfunctionMin = {implMin, "min", 1, MAX_ARG_COUNT};

static Status implMax(i16 argCount, Value *args, Value *out) {
  Value best = args[0];
  i16 i;
  for (i = 1; i < argCount; i++) {
    if (isNil(args[i])) {
      break;
    }
    if (valueLessThan(best, args[i])) {
      best = args[i];
    }
  }
  *out = best;
  return STATUS_OK;
}

static CFunction cfunctionMax = {implMax, "max", 1, MAX_ARG_COUNT};

static Status implSorted(i16 argCount, Value *args, Value *out) {
  ObjList *list;
  if (!newListFromIterable(args[0], &list)) {
    return STATUS_ERROR;
  }
  push(LIST_VAL(list));
  if (!sortListWithKeyFunc(list, argCount > 1 ? args[1] : NIL_VAL())) {
    return STATUS_ERROR;
  }
  *out = pop(); /* list */
  return STATUS_OK;
}

static CFunction cfunctionSorted = {implSorted, "sorted", 1, 2};

static Status implSet(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = newDict();
  Value iterator;
  push(DICT_VAL(dict));
  if (!valueFastIter(args[0], &iterator)) {
    return STATUS_ERROR;
  }
  push(iterator);
  for (;;) {
    Value key;
    if (!valueFastIterNext(&iterator, &key)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(key)) {
      break;
    }
    push(key);
    mapSet(&dict->map, key, NIL_VAL());
    pop(); /* key */
  }
  pop();        /* iterator */
  *out = pop(); /* dict */
  return STATUS_OK;
}

static CFunction cfunctionSet = {implSet, "Set", 1};

static Status implTuple(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList = copyFrozenList(args, argCount);
  *out = FROZEN_LIST_VAL(frozenList);
  return STATUS_OK;
}

static CFunction cfunctionTuple = {implTuple, "Tuple", 1, 4};

static Status implPrint(i16 argCount, Value *args, Value *out) {
  Value strVal;
  if (!implStr(argCount, args, &strVal)) {
    return STATUS_ERROR;
  }
  oprintln("%s", asString(strVal)->chars);
  return STATUS_OK;
}

static CFunction cfunctionPrint = {implPrint, "print", 1};

static Status implRange(i16 argCount, Value *args, Value *out) {
  double start = 0, stop, step = 1;
  i32 i = 0;
  for (i = 0; i < argCount; i++) {
    if (!isNumber(args[i])) {
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
  }
  *out = OBJ_VAL_EXPLICIT((Obj *)newRange(start, stop, step));
  return STATUS_OK;
}

static CFunction cfunctionRange = {implRange, "range", 1, 3};

static Status implFloat(i16 argCount, Value *args, Value *out) {
  Value arg = args[0];
  if (isNumber(arg)) {
    *out = arg;
    return STATUS_OK;
  }
  if (isString(arg)) {
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
        return STATUS_OK;
      }
    }
    runtimeError("Could not convert string to float: %s", str->chars);
    return STATUS_ERROR;
  }
  runtimeError("%s is not convertible to float", getKindName(arg));
  return STATUS_ERROR;
}

static CFunction funcFloat = {implFloat, "float", 1};

static Status implInt(i16 argCount, Value *args, Value *out) {
  Value arg = args[0];
  if (isNumber(arg)) {
    *out = NUMBER_VAL(floor(asNumber(arg)));
    return STATUS_OK;
  }
  if (isString(arg)) {
    String *str = (String *)arg.as.obj;
    i32 base = argCount > 1 ? asI32(args[1]) : 10;
    const char *ptr = str->chars;
    double value = 0, sign = 1;
    if (base < 2) {
      runtimeError("int(): unsupported base %d", (int)base);
      return STATUS_ERROR;
    }
    if (*ptr == '-' || *ptr == '+') {
      sign = *ptr == '-' ? -1 : 1;
      ptr++;
    }
    if (*ptr == '\0') {
      runtimeError("int(): Expected digit, but got end of string");
      return STATUS_ERROR;
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
        return STATUS_ERROR;
      }
      value = value * base + digit;
    }
    if (*ptr != '\0') {
      runtimeError("int(): Expected digit but got '%c'", *ptr);
      return STATUS_ERROR;
    }
    *out = NUMBER_VAL(value * sign);
    return STATUS_OK;
  }
  runtimeError("%s is not convertible to int", getKindName(arg));
  return STATUS_ERROR;
}

static CFunction funcInt = {implInt, "int", 1, 2};

static Status implIsClose(i16 argc, Value *args, Value *out) {
  Value a = args[0];
  Value b = args[1];
  double relTol = argc > 2 ? asNumber(args[2]) : DEFAULT_RELATIVE_TOLERANCE;
  double absTol = argc > 3 ? asNumber(args[3]) : DEFAULT_ABSOLUTE_TOLERANCE;
  if (isNumber(args[0]) && isNumber(args[1])) {
    *out = BOOL_VAL(doubleIsCloseEx(asNumber(a), asNumber(b), relTol, absTol));
    return STATUS_OK;
  }
  runtimeError(
      "Expectecd two Numbers, but got %s and %s",
      getKindName(a), getKindName(b));
  return STATUS_ERROR;
}

static CFunction funcIsClose = {implIsClose, "isClose", 2, 4};

static Status implSin(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(sin(asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcSin = {implSin, "sin", 1};

static Status implCos(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(cos(asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcCos = {implCos, "cos", 1};

static Status implTan(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(tan(asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcTan = {implTan, "tan", 1};

static Status implAbs(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  *out = NUMBER_VAL(value < 0 ? -value : value);
  return STATUS_OK;
}

static CFunction funcAbs = {implAbs, "abs", 1};

static Status implLog(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  if (!(value > 0)) { /* The '!' is to account for NaN */
    runtimeError("Argument to log() must be positive but got %f", value);
    return STATUS_ERROR;
  }
  *out = NUMBER_VAL(log(value));
  return STATUS_OK;
}

static CFunction funcLog = {implLog, "log", 1};

static Status implFlog2(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  i32 ret = 0;
  if (!(value > 0)) { /* The '!' is to account for NaN */
    runtimeError("Argument to flog2() must be positive but got %f", value);
    return STATUS_ERROR;
  }
  while (value > 1) {
    value /= 2;
    ret++;
  }
  *out = NUMBER_VAL(ret);
  return STATUS_OK;
}

static CFunction funcFlog2 = {implFlog2, "flog2", 1};

static Status implIsInstance(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(asClass(args[1]) == getClassOfValue(args[0]));
  return STATUS_OK;
}

static CFunction funcIsInstance = {implIsInstance, "isinstance", 2};

static Status implSort(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[0]);
  ObjList *keys =
      argCount < 2      ? NULL
      : isNil(args[1]) ? NULL
                        : asList(args[1]);
  sortList(list, keys);
  return STATUS_OK;
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
