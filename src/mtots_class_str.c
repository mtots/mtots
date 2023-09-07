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

static ubool implStringIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjStringIterator *si = (ObjStringIterator *)AS_OBJ(args[-1]);
  if (si->i >= si->string->codePointCount) {
    *out = STOP_ITERATION_VAL();
  } else if (si->string->utf32) {
    String *str;
    if (!internUTF32(si->string->utf32 + si->i++, 1, &str)) {
      return UFALSE;
    }
    *out = STRING_VAL(str);
  } else {
    *out = STRING_VAL(internString(si->string->chars + si->i++, 1));
  }
  return UTRUE;
}

static CFunction funcStringIteratorCall = {implStringIteratorCall, "__call__"};

static NativeObjectDescriptor descriptorStringIterator = {
    blackenStringIterator,
    nopFree,
    sizeof(ObjStringIterator),
    "StringIterator",
};

static ubool implStringIter(i16 argCount, Value *args, Value *out) {
  String *string = AS_STRING(args[-1]);
  ObjStringIterator *iterator = NEW_NATIVE(ObjStringIterator, &descriptorStringIterator);
  iterator->string = string;
  iterator->i = 0;
  *out = OBJ_VAL_EXPLICIT((Obj *)iterator);
  return UTRUE;
}

static CFunction funcStringIter = {implStringIter, "__iter__"};

static ubool implStrGetByteLength(i16 argCount, Value *args, Value *out) {
  String *str = AS_STRING(args[-1]);
  *out = NUMBER_VAL(str->byteLength);
  return UTRUE;
}

static CFunction funcStrGetByteLength = {implStrGetByteLength, "getByteLength"};

static ubool implStrGetItem(i16 argCount, Value *args, Value *out) {
  String *str = AS_STRING(args[-1]);
  size_t index = AS_INDEX(args[0], str->codePointCount);
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
  return UTRUE;
}

static CFunction funcStrGetItem = {implStrGetItem, "__getitem__", 1, 0, argsNumbers};

static ubool implStrSlice(i16 argCount, Value *args, Value *out) {
  String *str = AS_STRING(args[-1]), *slicedStr;
  i32 lower = IS_NIL(args[0]) ? 0 : AS_INDEX_LOWER(args[0], str->codePointCount);
  i32 upper = IS_NIL(args[1]) ? str->codePointCount : AS_INDEX_UPPER(args[1], str->codePointCount);
  if (!sliceString(str, lower, upper, &slicedStr)) {
    return UFALSE;
  }
  *out = STRING_VAL(slicedStr);
  return UTRUE;
}

static TypePattern argsStrSlice[] = {
    {TYPE_PATTERN_NUMBER_OR_NIL},
    {TYPE_PATTERN_NUMBER_OR_NIL},
};

static CFunction funcStrSlice = {implStrSlice, "__slice__", 2, 0, argsStrSlice};

static ubool implStrMod(i16 argCount, Value *args, Value *out) {
  const char *fmt = AS_CSTRING(args[-1]);
  ObjList *arglist = AS_LIST(args[0]);
  Buffer buf;

  initBuffer(&buf);

  if (!strMod(&buf, fmt, arglist)) {
    return UFALSE;
  }

  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);

  return UTRUE;
}

static TypePattern argsStrMod[] = {
    {TYPE_PATTERN_LIST},
};

static CFunction funcStrMod = {implStrMod, "__mod__", 1, 0, argsStrMod};

static char DEFAULT_STRIP_SET[] = " \t\r\n";

static ubool containsChar(const char *charSet, char ch) {
  for (; *charSet != '\0'; charSet++) {
    if (*charSet == ch) {
      return UTRUE;
    }
  }
  return UFALSE;
}

static ubool implStrStrip(i16 argCount, Value *args, Value *out) {
  String *str = AS_STRING(args[-1]);
  const char *stripSet = DEFAULT_STRIP_SET;
  const char *start = str->chars;
  const char *end = str->chars + str->byteLength;
  if (argCount > 0) {
    stripSet = AS_STRING(args[0])->chars;
  }
  while (start < end && containsChar(stripSet, start[0])) {
    start++;
  }
  while (start < end && containsChar(stripSet, end[-1])) {
    end--;
  }
  *out = STRING_VAL(internString(start, end - start));
  return UTRUE;
}

static CFunction funcStrStrip = {implStrStrip, "strip", 0, 1, argsStrings};

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

static ubool implStrReplace(i16 argCount, Value *args, Value *out) {
  String *orig = AS_STRING(args[-1]);
  String *oldstr = AS_STRING(args[0]);
  String *newstr = AS_STRING(args[1]);
  size_t len = cStrReplace(orig->chars, oldstr->chars, newstr->chars, NULL);
  char *chars = malloc(sizeof(char) * (len + 1));
  cStrReplace(orig->chars, oldstr->chars, newstr->chars, chars);
  chars[len] = '\0';
  *out = STRING_VAL(internOwnedString(chars, len));
  return UTRUE;
}

static CFunction funcStrReplace = {implStrReplace, "replace", 2, 0,
                                   argsStrings};

static ubool implStrJoin(i16 argCount, Value *args, Value *out) {
  String *sep = AS_STRING(args[-1]);
  ObjList *list = AS_LIST(args[0]);
  size_t i, len = (list->length - 1) * sep->byteLength;
  char *chars, *p;
  for (i = 0; i < list->length; i++) {
    if (!IS_STRING(list->buffer[i])) {
      runtimeError(
          "String.join() requires a list of strings, but found %s in the list",
          getKindName(list->buffer[i]));
      return UFALSE;
    }
    len += AS_STRING(list->buffer[i])->byteLength;
  }
  chars = p = malloc(sizeof(char) * (len + 1));
  for (i = 0; i < list->length; i++) {
    String *item = AS_STRING(list->buffer[i]);
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
  return UTRUE;
}

static TypePattern argsStrJoin[] = {
    {TYPE_PATTERN_LIST},
};

static CFunction funcStrJoin = {
    implStrJoin,
    "join",
    1,
    0,
    argsStrJoin,
};

static ubool implStringUpper(i16 argc, Value *args, Value *out) {
  /* Simple uppercase algorithm - converts ASCII lower case characters to their
   * corresponding uppercase characters */
  /* TODO: Consider uppercasing more generally with unicode as Python does. */
  String *string = AS_STRING(args[-1]);
  if (string->utf32) {
    /* TODO: */
    runtimeError("String.upper() not yet supported for non-ASCII strings");
    return UFALSE;
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
    return UTRUE;
  }
}

static CFunction funcStringUpper = {implStringUpper, "upper"};

static ubool implStringLower(i16 argc, Value *args, Value *out) {
  /* Simple lowercase algorithm - converts ASCII lower case characters to their
   * corresponding lowercase characters */
  /* TODO: Consider lowercasing more generally with unicode as Python does. */
  String *string = AS_STRING(args[-1]);
  if (string->utf32) {
    /* TODO: */
    runtimeError("String.lower() not yet supported for non-ASCII strings");
    return UFALSE;
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
    return UTRUE;
  }
}

static CFunction funcStringLower = {implStringLower, "lower"};

static ubool implStringStartsWith(i16 argc, Value *args, Value *out) {
  String *string = AS_STRING(args[-1]);
  String *prefix = AS_STRING(args[0]);
  *out = BOOL_VAL(
      string->byteLength >= prefix->byteLength &&
      memcmp(string->chars, prefix->chars, prefix->byteLength) == 0);
  return UTRUE;
}

static CFunction funcStringStartsWith = {implStringStartsWith, "startsWith", 1, 0, argsStrings};

static ubool implStringEndsWith(i16 argc, Value *args, Value *out) {
  String *string = AS_STRING(args[-1]);
  String *suffix = AS_STRING(args[0]);
  *out = BOOL_VAL(
      string->byteLength >= suffix->byteLength &&
      memcmp(
          string->chars + (string->byteLength - suffix->byteLength),
          suffix->chars,
          suffix->byteLength) == 0);
  return UTRUE;
}

static CFunction funcStringEndsWith = {implStringEndsWith, "endsWith", 1, 0, argsStrings};

static ubool padStringImpl(
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
      return UFALSE;
    }
    bputstrlen(out, padString->chars, remain);
  }
  if (padStart) {
    bputstrlen(out, string->chars, string->byteLength);
  }
  return UTRUE;
}

static ubool implStringPadX(ubool padStart, i16 argCount, Value *args, Value *out) {
  String *string = AS_STRING(args[-1]);
  size_t width = AS_SIZE(args[0]);
  String *padString = argCount > 1 ? AS_STRING(args[1]) : internCString(" ");
  Buffer buf;
  if (string->codePointCount >= width) {
    *out = STRING_VAL(string);
    return UTRUE;
  }
  push(STRING_VAL(padString));
  initBuffer(&buf);
  if (!padStringImpl(string, width, padString, padStart, &buf)) {
    freeBuffer(&buf);
    return UFALSE;
  }
  pop(); /* padString */
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static TypePattern argsStringPadX[] = {
    {TYPE_PATTERN_NUMBER},
    {TYPE_PATTERN_STRING},
};

static ubool implStringPadStart(i16 argCount, Value *args, Value *out) {
  return implStringPadX(UTRUE, argCount, args, out);
}

static CFunction funcStringPadStart = {
    implStringPadStart,
    "padStart",
    1,
    2,
    argsStringPadX,
};

static ubool implStringPadEnd(i16 argCount, Value *args, Value *out) {
  return implStringPadX(UFALSE, argCount, args, out);
}

static CFunction funcStringPadEnd = {
    implStringPadEnd,
    "padEnd",
    1,
    2,
    argsStringPadX,
};

static ubool implStringStaticFromUTF8(i16 argc, Value *args, Value *out) {
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  String *string = internString((char *)buffer->handle.data, buffer->handle.length);
  *out = STRING_VAL(string);
  return UTRUE;
}

static TypePattern argsStringStaticFromUTF8[] = {
    {TYPE_PATTERN_BUFFER},
};

static CFunction funcStringStaticFromUTF8 = {
    implStringStaticFromUTF8,
    "fromUTF8",
    1,
    0,
    argsStringStaticFromUTF8,
};

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
        TYPE_PATTERN_STRING,
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
