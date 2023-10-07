#include "mtots_globals.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_vm.h"

static Status implClock(i16 argCount, Value *args, Value *out) {
  *out = valNumber(clock() / (double)CLOCKS_PER_SEC);
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

static Status implLen(i16 argCount, Value *args, Value *out) {
  Value recv = args[0];
  size_t len;
  if (!valueLen(recv, &len)) {
    return STATUS_ERROR;
  }
  *out = valNumber(len);
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
  *out = valNumber(total);
  return STATUS_OK;
}

static CFunction cfuncSum = {implSum, "sum", 1};

static void reverseChars(char *chars, size_t start, size_t end) {
  while (start + 1 < end) {
    char tmp = chars[start];
    chars[start] = chars[end - 1];
    chars[end - 1] = tmp;
    start++;
    end--;
  }
}

static Status implHex(i16 argCount, Value *args, Value *out) {
  double rawValue = asNumber(args[0]);
  size_t start, end, value;
  StringBuilder sb;

  initStringBuilder(&sb);
  if (rawValue < 0) {
    sbputchar(&sb, '-');
    rawValue *= -1;
  }
  value = (size_t)rawValue;
  sbputstr(&sb, "0x");
  if (value == 0) {
    sbputchar(&sb, '0');
  } else {
    start = sb.length;
    while (value) {
      size_t digit = value & 0xF;
      if (digit < 10) {
        sbputchar(&sb, '0' + digit);
      } else {
        sbputchar(&sb, 'A' + (digit - 10));
      }
      value /= 0x10;
    }
    end = sb.length;
    reverseChars(sb.buffer, start, end);
  }

  *out = valString(sbstring(&sb));
  freeStringBuilder(&sb);
  return STATUS_OK;
}

static CFunction funcHex = {implHex, "hex", 1};

static Status implOct(i16 argCount, Value *args, Value *out) {
  double rawValue = asNumber(args[0]);
  size_t start, end, value;
  StringBuilder sb;

  initStringBuilder(&sb);
  if (rawValue < 0) {
    sbputchar(&sb, '-');
    rawValue *= -1;
  }
  value = (size_t)rawValue;
  sbputstr(&sb, "0o");
  if (value == 0) {
    sbputchar(&sb, '0');
  } else {
    start = sb.length;
    while (value) {
      sbputchar(&sb, '0' + (value & 7));
      value /= 8;
    }
    end = sb.length;
    reverseChars(sb.buffer, start, end);
  }

  *out = valString(sbstring(&sb));
  freeStringBuilder(&sb);
  return STATUS_OK;
}

static CFunction funcOct = {implOct, "oct", 1};

static Status implBin(i16 argCount, Value *args, Value *out) {
  double rawValue = asNumber(args[0]);
  size_t start, end, value;
  StringBuilder sb;

  initStringBuilder(&sb);
  if (rawValue < 0) {
    sbputchar(&sb, '-');
    rawValue *= -1;
  }
  value = (size_t)rawValue;
  sbputstr(&sb, "0b");
  if (value == 0) {
    sbputchar(&sb, '0');
  } else {
    start = sb.length;
    while (value) {
      sbputchar(&sb, (value & 1) ? '1' : '0');
      value /= 2;
    }
    end = sb.length;
    reverseChars(sb.buffer, start, end);
  }

  *out = valString(sbstring(&sb));
  freeStringBuilder(&sb);
  return STATUS_OK;
}

static CFunction funcBin = {implBin, "bin", 1};

static Status implRound(i16 argCount, Value *args, Value *out) {
  double number = asNumber(args[0]);
  *out = valNumber((long)(number + 0.5));
  return STATUS_OK;
}

static CFunction cfuncRound = {implRound, "round", 0, 1};

static Status implType(i16 argCount, Value *args, Value *out) {
  *out = valClass(getClassOfValue(args[0]));
  return STATUS_OK;
}

static CFunction cfuncType = {implType, "type", 1};

static Status implRepr(i16 argCount, Value *args, Value *out) {
  StringBuilder sb;
  initStringBuilder(&sb);
  if (!valueRepr(&sb, args[0])) {
    freeStringBuilder(&sb);
    return STATUS_ERROR;
  }
  *out = valString(sbstring(&sb));
  freeStringBuilder(&sb);
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
  *out = valString(internString(&c, 1));
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
  *out = valNumber((u8)str->chars[0]);
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
  push(valList(list));
  if (!sortListWithKeyFunc(list, argCount > 1 ? args[1] : valNil())) {
    return STATUS_ERROR;
  }
  *out = pop(); /* list */
  return STATUS_OK;
}

static CFunction cfunctionSorted = {implSorted, "sorted", 1, 2};

static Status implSet(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = newDict();
  Value iterator;
  push(valDict(dict));
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
    mapSet(&dict->map, key, valNil());
    pop(); /* key */
  }
  pop();        /* iterator */
  *out = pop(); /* dict */
  return STATUS_OK;
}

static CFunction cfunctionSet = {implSet, "Set", 1};

static Status implTuple(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList = copyFrozenList(args, argCount);
  *out = valFrozenList(frozenList);
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
  Range range;
  range.start = 0;
  range.step = 1;
  switch (argCount) {
    case 1:
      range.stop = asI32(args[0]);
      break;
    case 2:
      range.start = asI32(args[0]);
      range.stop = asI32(args[1]);
      break;
    case 3:
      range.start = asI32(args[0]);
      range.stop = asI32(args[1]);
      range.step = asI32(args[2]);
      break;
    default:
      panic("Invalid argc to range() (%d)", argCount);
  }
  *out = valRange(range);
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
        *out = valNumber(strtod(str->chars, NULL));
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
    *out = valNumber(floor(asNumber(arg)));
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
    *out = valNumber(value * sign);
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
    *out = valBool(doubleIsCloseEx(asNumber(a), asNumber(b), relTol, absTol));
    return STATUS_OK;
  }
  runtimeError(
      "Expectecd two Numbers, but got %s and %s",
      getKindName(a), getKindName(b));
  return STATUS_ERROR;
}

static CFunction funcIsClose = {implIsClose, "isClose", 2, 4};

static Status implSin(i16 argCount, Value *args, Value *out) {
  *out = valNumber(sin(asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcSin = {implSin, "sin", 1};

static Status implCos(i16 argCount, Value *args, Value *out) {
  *out = valNumber(cos(asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcCos = {implCos, "cos", 1};

static Status implTan(i16 argCount, Value *args, Value *out) {
  *out = valNumber(tan(asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcTan = {implTan, "tan", 1};

static Status implAbs(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  *out = valNumber(value < 0 ? -value : value);
  return STATUS_OK;
}

static CFunction funcAbs = {implAbs, "abs", 1};

static Status implLog(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[0]);
  if (!(value > 0)) { /* The '!' is to account for NaN */
    runtimeError("Argument to log() must be positive but got %f", value);
    return STATUS_ERROR;
  }
  *out = valNumber(log(value));
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
  *out = valNumber(ret);
  return STATUS_OK;
}

static CFunction funcFlog2 = {implFlog2, "flog2", 1};

static Status implIsInstance(i16 argCount, Value *args, Value *out) {
  *out = valBool(asClass(args[1]) == getClassOfValue(args[0]));
  return STATUS_OK;
}

static CFunction funcIsInstance = {implIsInstance, "isinstance", 2};

static Status implSort(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[0]);
  ObjList *keys =
      argCount < 2     ? NULL
      : isNil(args[1]) ? NULL
                       : asList(args[1]);
  sortList(list, keys);
  return STATUS_OK;
}

static CFunction funcSort = {implSort, "__sort__", 1, 2};

typedef struct TryState {
  Value *stackTop;
  i16 frameCount;
} TryState;

static void saveTryState(TryState *state) {
  state->stackTop = vm.stackTop;
  state->frameCount = vm.frameCount;
}

static void restoreTryState(TryState *state) {
  closeUpvalues(state->stackTop);
  vm.stackTop = state->stackTop;
  vm.frameCount = state->frameCount;
}

static Status tryCatchWithoutFinally(Value tryFunc, Value catchFunc, TryState *state) {
  push(tryFunc);
  if (callFunction(0)) {
    /* tryFunc encountered no errors */
    return STATUS_OK;
  }
  if (!isNil(catchFunc)) {
    /* attempt to save with a catchFunc */
    restoreTryState(state);
    saveCurrentErrorString();
    push(catchFunc);
    if (callFunction(0)) {
      /* catchFunc handled the exception successfully */
      return STATUS_OK;
    }
  }
  /* there is a pending error to be propagated */
  return STATUS_ERROR;
}

static Status implTryCatch(i16 argc, Value *argv, Value *out) {
  /* WARNING: this is an extra hairy function - since tryCatch
   * needs to know how to recover when we enter potentially
   * inconsistent states, this function needs to poke at internals
   * that most other CFunctions wouldn't know about */
  Value tryFunc = argv[0];
  Value catchFunc = argc > 1 && !isNil(argv[1]) ? argv[1] : valNil();
  Value finallyFunc = argc > 2 && !isNil(argv[2]) ? argv[2] : valNil();
  TryState state;
  saveTryState(&state);

  if (tryCatchWithoutFinally(tryFunc, catchFunc, &state)) {
    /* try-catch had no errors or successfully recovered -
     * the return value should be on the top of stack at this point */
    if (!isNil(finallyFunc)) {
      /* if a finallyFunc is present, we need to run this */
      push(finallyFunc);
      if (!callFunction(0)) {
        return STATUS_ERROR;
      }
      pop(); /* return value (finallyFunc) */
    }
    *out = pop(); /* try-catch */
    return STATUS_OK;
  }

  /* if the try-catch still propagates an error, we need
   * to propagate it. However, we still need to run finallyFunc */
  if (!isNil(finallyFunc)) {
    restoreTryState(&state);

    /* we need to store the error string, because finallyFunc could
     * clobber it (with an inner tryCatch of its own) */
    push(valString(internCString(getErrorString())));

    push(finallyFunc);
    if (callFunction(0)) {
      pop(); /* return value (finallyFunc) */

      /* If finallyFunc does not throw, we need to restore
       * the previous exception -
       * pop the value we pushed earlier with getErrorString() */
      setErrorString(asString(pop())->chars);
    }
  }
  return STATUS_ERROR;
}

static CFunction funcTryCatch = {implTryCatch, "tryCatch", 2, 3};

static Status implGetErrorString(i16 argc, Value *argv, Value *out) {
  const char *errorString = getSavedErrorString();
  if (errorString == NULL) {
    runtimeError("No error string found");
    return STATUS_ERROR;
  }
  *out = valString(internCString(errorString));
  clearSavedErrorString();
  return STATUS_OK;
}

static CFunction cfuncGetErrorString = {implGetErrorString, "getErrorString"};

void defineDefaultGlobals(void) {
  NativeObjectDescriptor *descriptors[] = {
      &descriptorStringBuilder,
      NULL,
  },
                         **descriptor;
  CFunction *functions[] = {
      &cfunctionClock,
      &cfuncExit,
      &cfuncLen,
      &cfuncSum,
      &funcHex,
      &funcOct,
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
      &funcTryCatch,
      &cfuncGetErrorString,
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
      &vm.vectorClass,
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

  defineGlobal("PI", valNumber(PI));
  defineGlobal("TAU", valNumber(TAU));

#ifdef NAN
  defineGlobal("NAN", valNumber(NAN));
#else
  defineGlobal("NAN", valNumber(0.0 / 0.0));
#endif

#ifdef INFINITY
  defineGlobal("INFINITY", valNumber(INFINITY));
#else
  defineGlobal("INFINITY", valNumber(1.0 / 0.0));
#endif

  defineGlobal("StopIteration", valStopIteration());

  for (function = functions; *function; function++) {
    defineGlobal((*function)->name, valCFunction(*function));
  }
  defineGlobal("__print", valCFunction(&cfunctionPrint));

  for (builtinClass = builtinClasses; *builtinClass; builtinClass++) {
    mapSetStr(&vm.globals, (**builtinClass)->name, valClass(**builtinClass));
  }

  for (descriptor = descriptors; *descriptor; descriptor++) {
    NativeObjectDescriptor *d = *descriptor;
    mapSetStr(&vm.globals, d->klass->name, valClass(d->klass));
  }
}
