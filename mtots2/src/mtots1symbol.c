#include "mtots1symbol.h"

#include <stdlib.h>
#include <string.h>

#define MAX_LOAD 0.75

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

struct Symbol {
  u8 *utf8;
  size_t byteLength;
  u32 hash;
};

typedef struct SymbolSet {
  size_t capacity;  /* 0 or (8 * <power of 2>) */
  size_t occupancy; /* number of live symbols in 'entries' */
  size_t threshold; /* The next occupancy threshold that will trigger a resize */
  Symbol **entries;
} SymbolSet;

static SymbolSet symbolSet;

u32 hashStringData(const u8 *key, size_t length) {
  /* FNV-1a as presented in the Crafting Interpreters book */
  size_t i;
  u32 hash = 2166136261u;
  for (i = 0; i < length; i++) {
    hash ^= key[i];
    hash *= 16777619;
  }
  return hash;
}

/* NOTE: Capacity must be non-zero when called */
static Symbol **find(Symbol **entries, size_t capacity, const u8 *chars, size_t length, u32 hash) {
  /* OPTIMIZED hash % capacity (requires capacity is a power of 2) */
  u32 index = hash & (u32)(capacity - 1);
  for (;;) {
    Symbol *entry = entries[index];
    if (!entry ||
        (entry->hash == hash &&
         entry->byteLength == length &&
         (entry->utf8 == chars || strcmp((const char *)entry->utf8, (const char *)chars) == 0))) {
      return entries + index;
    }
    /* OPTIMIZED hash % capacity (requires capacity is a power of 2) */
    index = (index + 1) & (capacity - 1);
  }
}

static void adjustCapacity(size_t newCapacity) {
  size_t oldCapacity = symbolSet.capacity, i;
  Symbol **oldEntries = symbolSet.entries;

  /* NOTE: Assumption here, zeroed out == NULL, is not guaranteed by ANSI C89 */
  symbolSet.entries = (Symbol **)calloc(newCapacity, sizeof(Symbol *));
  symbolSet.capacity = newCapacity;
  symbolSet.threshold = (size_t)(newCapacity * MAX_LOAD);

  for (i = 0; i < oldCapacity; i++) {
    Symbol *symbol = oldEntries[i];
    if (symbol) {
      Symbol **entry = find(
          symbolSet.entries,
          symbolSet.capacity,
          symbol->utf8,
          symbol->byteLength,
          symbol->hash);
      *entry = symbol;
    }
  }
}

static Symbol *intern(const u8 *chars, size_t length, u32 hash) {
  Symbol **entry;
  if (symbolSet.occupancy >= symbolSet.threshold) {
    adjustCapacity(GROW_CAPACITY(symbolSet.capacity));
  }
  entry = find(symbolSet.entries, symbolSet.capacity, chars, length, hash);
  if (*entry) {
    return *entry;
  }
  {
    Symbol *symbol = (Symbol *)malloc(sizeof(Symbol));
    symbol->utf8 = (u8 *)malloc(length + 1);
    memcpy(symbol->utf8, chars, length);
    symbol->utf8[length] = '\0';
    symbol->byteLength = length;
    symbol->hash = hash;
    *entry = symbol;
    symbolSet.occupancy++;
    return symbol;
  }
}

Symbol *newSymbol(const char *s) {
  size_t len = strlen(s);
  return intern((const u8 *)s, len, hashStringData((const u8 *)s, len));
}

const char *symbolChars(Symbol *symbol) {
  return (char *)symbol->utf8;
}

size_t symbolByteLength(Symbol *symbol) {
  return symbol->byteLength;
}

u32 symbolHash(Symbol *symbol) {
  return symbol->hash;
}
