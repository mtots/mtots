#include "mtots_class_str.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

typedef struct ObjStringIterator {
  ObjNative obj;
  String *string;
  size_t i;
} ObjStringIterator;

static void blackenStringIterator(ObjNative *n) {
  ObjStringIterator *si = (ObjStringIterator *)n;
  markString(si->string);
}

static Status implStringIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjStringIterator *si = (ObjStringIterator *)AS_OBJ_UNSAFE(args[-1]);
  if (si->i >= si->string->codePointCount) {
    *out = STOP_ITERATION_VAL();
  } else if (si->string->utf32) {
    String *str;
    if (!internUTF32(si->string->utf32 + si->i++, 1, &str)) {
      return STATUS_ERROR;
    }
    *out = STRING_VAL(str);
  } else {
    *out = STRING_VAL(internString(si->string->chars + si->i++, 1));
  }
  return STATUS_OK;
}

static CFunction funcStringIteratorCall = {implStringIteratorCall, "__call__"};

static NativeObjectDescriptor descriptorStringIterator = {
    blackenStringIterator,
    nopFree,
    sizeof(ObjStringIterator),
    "StringIterator",
};

static Status implStringIter(i16 argCount, Value *args, Value *out) {
  String *string = asString(args[-1]);
  ObjStringIterator *iterator = NEW_NATIVE(ObjStringIterator, &descriptorStringIterator);
  iterator->string = string;
  iterator->i = 0;
  *out = OBJ_VAL_EXPLICIT((Obj *)iterator);
  return STATUS_OK;
}

static CFunction funcStringIter = {implStringIter, "__iter__"};

static Status implStrGetByteLength(i16 argCount, Value *args, Value *out) {
  String *str = asString(args[-1]);
  *out = NUMBER_VAL(str->byteLength);
  return STATUS_OK;
}

static CFunction funcStrGetByteLength = {implStrGetByteLength, "getByteLength"};

static Status implStrGetItem(i16 argCount, Value *args, Value *out) {
  String *str = asString(args[-1]);
  size_t index = asIndex(args[0], str->codePointCount);
  if (str->utf32) {
    u32 codePoint = str->utf32[index];
    char buffer[5];
    int bytesWritten = encodeUTF8Char(codePoint, buffer);
    if (bytesWritten == 0) {
      panic("Invalid code point %lu\n", (unsigned long)codePoint);
    }
    *out = STRING_VAL(internString(buffer, bytesWritten));
  } else {
    *out = STRING_VAL(internString(str->chars + index, 1));
  }
  return STATUS_OK;
}

static CFunction funcStrGetItem = {implStrGetItem, "__getitem__", 1};

static Status implStrSlice(i16 argCount, Value *args, Value *out) {
  String *str = asString(args[-1]), *slicedStr;
  i32 lower = isNil(args[0]) ? 0 : asIndexLower(args[0], str->codePointCount);
  i32 upper = isNil(args[1]) ? str->codePointCount : asIndexUpper(args[1], str->codePointCount);
  if (!sliceString(str, lower, upper, &slicedStr)) {
    return STATUS_ERROR;
  }
  *out = STRING_VAL(slicedStr);
  return STATUS_OK;
}

static CFunction funcStrSlice = {implStrSlice, "__slice__", 2};

static Status implStrMod(i16 argCount, Value *args, Value *out) {
  const char *fmt = asString(args[-1])->chars;
  ObjList *arglist = asList(args[0]);
  Buffer buf;

  initBuffer(&buf);

  if (!strMod(&buf, fmt, arglist)) {
    return STATUS_ERROR;
  }

  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);

  return STATUS_OK;
}

static CFunction funcStrMod = {implStrMod, "__mod__", 1, 0};

static char DEFAULT_STRIP_SET[] = " \t\r\n";

static ubool containsChar(const char *charSet, char ch) {
  for (; *charSet != '\0'; charSet++) {
    if (*charSet == ch) {
      return STATUS_OK;
    }
  }
  return STATUS_ERROR;
}

static Status implStrStrip(i16 argCount, Value *args, Value *out) {
  String *str = asString(args[-1]);
  const char *stripSet = DEFAULT_STRIP_SET;
  const char *start = str->chars;
  const char *end = str->chars + str->byteLength;
  if (argCount > 0) {
    stripSet = asString(args[0])->chars;
  }
  while (start < end && containsChar(stripSet, start[0])) {
    start++;
  }
  while (start < end && containsChar(stripSet, end[-1])) {
    end--;
  }
  *out = STRING_VAL(internString(start, end - start));
  return STATUS_OK;
}

static CFunction funcStrStrip = {implStrStrip, "strip", 0, 1};

static size_t cStrReplace(
    const char *s, const char *oldstr, const char *newstr, char *out) {
  char *outp = out;
  size_t len = 0, oldstrlen = strlen(oldstr), newstrlen = strlen(newstr);

  while (*s) {
    if (strncmp(s, oldstr, oldstrlen) == 0) {
      len += newstrlen;
      if (outp) {
        memcpy(outp, newstr, newstrlen);
        outp += newstrlen;
      }
      s += oldstrlen;
    } else {
      len++;
      if (outp) *outp++ = *s;
      s++;
    }
  }

  return len;
}

static Status implStrReplace(i16 argCount, Value *args, Value *out) {
  String *orig = asString(args[-1]);
  String *oldstr = asString(args[0]);
  String *newstr = asString(args[1]);
  size_t len = cStrReplace(orig->chars, oldstr->chars, newstr->chars, NULL);
  char *chars = malloc(sizeof(char) * (len + 1));
  cStrReplace(orig->chars, oldstr->chars, newstr->chars, chars);
  chars[len] = '\0';
  *out = STRING_VAL(internOwnedString(chars, len));
  return STATUS_OK;
}

static CFunction funcStrReplace = {implStrReplace, "replace", 2};

static Status implStrJoin(i16 argCount, Value *args, Value *out) {
  String *sep = asString(args[-1]);
  ObjList *list = asList(args[0]);
  size_t i, len = (list->length - 1) * sep->byteLength;
  char *chars, *p;
  for (i = 0; i < list->length; i++) {
    if (!isString(list->buffer[i])) {
      runtimeError(
          "String.join() requires a list of strings, but found %s in the list",
          getKindName(list->buffer[i]));
      return STATUS_ERROR;
    }
    len += asString(list->buffer[i])->byteLength;
  }
  chars = p = malloc(sizeof(char) * (len + 1));
  for (i = 0; i < list->length; i++) {
    String *item = asString(list->buffer[i]);
    if (i > 0) {
      memcpy(p, sep->chars, sep->byteLength);
      p += sep->byteLength;
    }
    memcpy(p, item->chars, item->byteLength);
    p += item->byteLength;
  }
  *p = '\0';
  if (p - chars != len) {
    panic("Consistency error in String.join()");
  }
  *out = STRING_VAL(internOwnedString(chars, len));
  return STATUS_OK;
}

static CFunction funcStrJoin = {implStrJoin, "join", 1};

static Status implStringUpper(i16 argc, Value *args, Value *out) {
  /* Simple uppercase algorithm - converts ASCII lower case characters to their
   * corresponding uppercase characters */
  /* TODO: Consider uppercasing more generally with unicode as Python does. */
  String *string = asString(args[-1]);
  if (string->utf32) {
    /* TODO: */
    runtimeError("String.upper() not yet supported for non-ASCII strings");
    return STATUS_ERROR;
  } else {
    size_t i;
    char *newChars = (char *)malloc(string->byteLength), *p = newChars;
    String *newString;
    for (i = 0; i < string->byteLength; i++) {
      char ch = string->chars[i];
      if ('a' <= ch && ch <= 'z') {
        *p++ = 'A' + (ch - 'a');
      } else {
        *p++ = ch;
      }
    }
    newString = internString(newChars, string->byteLength);
    free(newChars);
    *out = STRING_VAL(newString);
    return STATUS_OK;
  }
}

static CFunction funcStringUpper = {implStringUpper, "upper"};

static Status implStringLower(i16 argc, Value *args, Value *out) {
  /* Simple lowercase algorithm - converts ASCII lower case characters to their
   * corresponding lowercase characters */
  /* TODO: Consider lowercasing more generally with unicode as Python does. */
  String *string = asString(args[-1]);
  if (string->utf32) {
    /* TODO: */
    runtimeError("String.lower() not yet supported for non-ASCII strings");
    return STATUS_ERROR;
  } else {
    size_t i;
    char *newChars = (char *)malloc(string->byteLength), *p = newChars;
    String *newString;
    for (i = 0; i < string->byteLength; i++) {
      char ch = string->chars[i];
      if ('A' <= ch && ch <= 'Z') {
        *p++ = 'a' + (ch - 'A');
      } else {
        *p++ = ch;
      }
    }
    newString = internString(newChars, string->byteLength);
    free(newChars);
    *out = STRING_VAL(newString);
    return STATUS_OK;
  }
}

static CFunction funcStringLower = {implStringLower, "lower"};

static Status implStringStartsWith(i16 argc, Value *args, Value *out) {
  String *string = asString(args[-1]);
  String *prefix = asString(args[0]);
  *out = BOOL_VAL(
      string->byteLength >= prefix->byteLength &&
      memcmp(string->chars, prefix->chars, prefix->byteLength) == 0);
  return STATUS_OK;
}

static CFunction funcStringStartsWith = {implStringStartsWith, "startsWith", 1};

static Status implStringEndsWith(i16 argc, Value *args, Value *out) {
  String *string = asString(args[-1]);
  String *suffix = asString(args[0]);
  *out = BOOL_VAL(
      string->byteLength >= suffix->byteLength &&
      memcmp(
          string->chars + (string->byteLength - suffix->byteLength),
          suffix->chars,
          suffix->byteLength) == 0);
  return STATUS_OK;
}

static CFunction funcStringEndsWith = {implStringEndsWith, "endsWith", 1};

static Status padStringImpl(
    String *string,
    size_t width,
    String *padString,
    ubool padStart,
    Buffer *out) {
  size_t remain = width - string->codePointCount;
  if (!padStart) {
    bputstrlen(out, string->chars, string->byteLength);
  }
  while (remain >= padString->codePointCount) {
    remain -= padString->codePointCount;
    bputstrlen(out, padString->chars, padString->byteLength);
  }
  if (remain > 0) {
    if (padString->utf32) {
      runtimeError(
          "Non-ASCII paddings that do not evenly divide into the required "
          "remaining width are not supported");
      return STATUS_ERROR;
    }
    bputstrlen(out, padString->chars, remain);
  }
  if (padStart) {
    bputstrlen(out, string->chars, string->byteLength);
  }
  return STATUS_OK;
}

static Status implStringPadX(ubool padStart, i16 argCount, Value *args, Value *out) {
  String *string = asString(args[-1]);
  size_t width = asSize(args[0]);
  String *padString = argCount > 1 ? asString(args[1]) : internCString(" ");
  Buffer buf;
  if (string->codePointCount >= width) {
    *out = STRING_VAL(string);
    return STATUS_OK;
  }
  push(STRING_VAL(padString));
  initBuffer(&buf);
  if (!padStringImpl(string, width, padString, padStart, &buf)) {
    freeBuffer(&buf);
    return STATUS_ERROR;
  }
  pop(); /* padString */
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return STATUS_OK;
}

static Status implStringPadStart(i16 argCount, Value *args, Value *out) {
  return implStringPadX(UTRUE, argCount, args, out);
}

static CFunction funcStringPadStart = {implStringPadStart, "padStart", 1, 2};

static Status implStringPadEnd(i16 argCount, Value *args, Value *out) {
  return implStringPadX(UFALSE, argCount, args, out);
}

static CFunction funcStringPadEnd = {implStringPadEnd, "padEnd", 1, 2};

static Status implStringStaticFromUTF8(i16 argc, Value *args, Value *out) {
  ObjBuffer *buffer = asBuffer(args[0]);
  String *string = internString((char *)buffer->handle.data, buffer->handle.length);
  *out = STRING_VAL(string);
  return STATUS_OK;
}

static CFunction funcStringStaticFromUTF8 = {implStringStaticFromUTF8, "fromUTF8", 1};

void initStringClass(void) {
  {
    CFunction *methods[] = {
        &funcStringIter,
        &funcStrGetByteLength,
        &funcStrGetItem,
        &funcStrSlice,
        &funcStrMod,
        &funcStrStrip,
        &funcStrReplace,
        &funcStrJoin,
        &funcStringUpper,
        &funcStringLower,
        &funcStringStartsWith,
        &funcStringEndsWith,
        &funcStringPadStart,
        &funcStringPadEnd,
        NULL,
    };
    CFunction *staticMethods[] = {
        &funcStringStaticFromUTF8,
        NULL,
    };
    newBuiltinClass(
        "String",
        &vm.stringClass,
        methods,
        staticMethods);
  }

  {
    CFunction *methods[] = {
        &funcStringIteratorCall,
        NULL,
    };
    newNativeClass(NULL, &descriptorStringIterator, methods, NULL);
  }
}
