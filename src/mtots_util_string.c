#include "mtots_util_string.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_util_error.h"
#include "mtots_util_unicode.h"

#define STRING_SET_MAX_LOAD 0.75

typedef struct StringSet {
  String **strings;
  size_t capacity, occupied, allocationSize;
} StringSet;

static StringSet allStrings;

static ubool specialStringsInitialized;
static String *oneCharAsciiStrings[128];
static String *emptyString;

/* If called, the empty string and one character ASCII strings will
 * be cached. These strings will never be freed, and when calling
 * internString, these will be retrieved very quickly. */
void initSpecialStrings(void) {
  if (!specialStringsInitialized) {
    char ch;
    for (ch = 0; ((unsigned char)ch) < 128; ch++) {
      oneCharAsciiStrings[(size_t)ch] = internString(&ch, 1);
    }
    emptyString = internString(NULL, 0);
    specialStringsInitialized = UTRUE;
  }
}

static u32 hashString(const char *key, size_t length) {
  /* FNV-1a as presented in the Crafting Interpreters book */
  size_t i;
  u32 hash = 2166136261u;
  for (i = 0; i < length; i++) {
    hash ^= (u8)key[i];
    hash *= 16777619;
  }
  return hash;
}

static String **stringSetFindEntry(const char *chars, size_t length, u32 hash) {
  u32 index = hash & (allStrings.capacity - 1);
  for (;;) {
    String **entry = &allStrings.strings[index];
    String *str = *entry;
    if (str == NULL ||
        (str->byteLength == length &&
         str->hash == hash &&
         memcmp(str->chars, chars, length) == 0)) {
      return entry;
    }
    index = (index + 1) & (allStrings.capacity - 1);
  }
}

static void computeUnicodeMetadata(String *string) {
  const char *limit = string->chars + string->byteLength, *p;
  string->codePointCount = 0;
  for (p = string->chars; p < limit;) {
    u32 codePoint;
    int codePointLen = decodeUTF8Char(p, limit, &codePoint);
    if (codePointLen == 0) {
      panic("Tried to intern a string that is not valid UTF-8");
    }
    p += codePointLen;
    string->codePointCount++;
  }
  if (string->codePointCount != string->byteLength) {
    u32 *utf32 = string->utf32 = (u32 *)malloc(sizeof(u32) * string->codePointCount);
    for (p = string->chars; p < limit;) {
      p += decodeUTF8Char(p, limit, utf32++);
    }
  } else {
    string->utf32 = NULL;
  }
}

String *internString(const char *chars, size_t byteLength) {
  if (specialStringsInitialized) {
    if (byteLength == 0) {
      return emptyString;
    } else if (byteLength == 1 && ((unsigned char)chars[0]) < 128) {
      return oneCharAsciiStrings[(size_t)chars[0]];
    }
  }
  if (allStrings.occupied + 1 > allStrings.capacity * STRING_SET_MAX_LOAD) {
    size_t oldCap = allStrings.capacity;
    size_t newCap = oldCap < 8 ? 8 : oldCap * 2;
    size_t i;
    String **oldStrings = allStrings.strings;
    String **newStrings = (String **)malloc(sizeof(String *) * newCap);
    String **entry;
    for (i = 0; i < newCap; i++) {
      newStrings[i] = NULL;
    }
    allStrings.strings = newStrings;
    allStrings.capacity = newCap;
    allStrings.occupied = 0;
    allStrings.allocationSize = 0;
    for (i = 0; i < oldCap; i++) {
      String *oldString = oldStrings[i];
      if (oldString == NULL) {
        continue;
      }
      entry = stringSetFindEntry(oldString->chars, oldString->byteLength, oldString->hash);
      if (*entry) {
        assertionError("internString");
      }
      *entry = oldString;
      allStrings.occupied++;
      allStrings.allocationSize += sizeof(String) + oldString->byteLength;
    }
    free(oldStrings);
  }

  {
    u32 hash = hashString(chars, byteLength);
    String **entry = stringSetFindEntry(chars, byteLength, hash);
    String *string;
    if (*entry) {
      return *entry;
    }
    string = (String *)malloc(sizeof(String));
    string->isMarked = UFALSE;
    string->byteLength = byteLength;
    string->hash = hash;
    string->chars = (char *)malloc(byteLength + 1);
    memcpy(string->chars, chars, byteLength);
    string->chars[byteLength] = '\0';
    *entry = string;
    allStrings.occupied++;
    allStrings.allocationSize += sizeof(String) + string->byteLength;
    computeUnicodeMetadata(string);
    return string;
  }
}

String *internCString(const char *string) {
  return internString(string, strlen(string));
}

String *internOwnedString(char *chars, size_t length) {
  /* TODO actually reuse the memory provided by the caller */
  String *string = internString(chars, length);
  free(chars);
  return string;
}

Status internUTF32(const u32 *utf32, size_t codePointCount, String **out) {
  size_t byteLength = 0, i, j;
  char *utf8;
  for (i = 0; i < codePointCount; i++) {
    size_t charLength = encodeUTF8Char(utf32[i], NULL);
    if (charLength == 0) {
      runtimeError("internUTF32(): '%lu' is not a valid code point", (unsigned long)utf32[i]);
      return STATUS_ERROR;
    }
    byteLength += charLength;
  }
  utf8 = malloc(byteLength);
  for (i = j = 0; i < codePointCount; i++) {
    j += encodeUTF8Char(utf32[i], utf8 + j);
  }
  if (j != byteLength) {
    panic("internUTF32(): internal consistency error");
  }
  *out = internString(utf8, byteLength);
  free(utf8);
  return STATUS_OK;
}

Status sliceString(String *string, size_t start, size_t end, String **out) {
  if (end > string->codePointCount) {
    end = string->codePointCount;
  }
  if (end <= start) {
    *out = internString(NULL, 0);
    return STATUS_OK;
  }
  if (string->codePointCount == string->byteLength) {
    /* ASCII */
    *out = internString(string->chars + start, end - start);
    return STATUS_OK;
  }
  /* non-ASCII */
  return internUTF32(string->utf32 + start, end - start, out);
}

size_t getInternedStringsAllocationSize(void) {
  return allStrings.allocationSize;
}

void freeUnmarkedStrings(void) {
  size_t i, cap = allStrings.capacity;
  String **oldEntries = allStrings.strings;
  String **newEntries = (String **)malloc(sizeof(String *) * cap);
  if (specialStringsInitialized) {
    emptyString->isMarked = UTRUE;
    for (i = 0; i < 128; i++) {
      oneCharAsciiStrings[i]->isMarked = UTRUE;
    }
  }
  for (i = 0; i < cap; i++) {
    newEntries[i] = NULL;
  }
  allStrings.occupied = 0;
  allStrings.allocationSize = 0;
  allStrings.strings = newEntries;
  for (i = 0; i < cap; i++) {
    String *str = oldEntries[i];
    if (str) {
      if (str->isMarked) {
        String **entry = stringSetFindEntry(str->chars, str->byteLength, str->hash);
        if (*entry) {
          assertionError("freeUnmarkedStrings");
        }
        *entry = str;
        str->isMarked = UFALSE;
        allStrings.occupied++;
        allStrings.allocationSize += sizeof(String) + str->byteLength;
      } else {
        free(str->chars);
        free(str->utf32);
        free(str);
        oldEntries[i] = NULL;
      }
    }
  }
  free(oldEntries);
}
