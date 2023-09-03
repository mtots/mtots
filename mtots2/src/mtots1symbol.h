#ifndef mtots1symbol_h
#define mtots1symbol_h

#include "mtots0common.h"

/** Symbols are interned strings that can be easily check for equality
 * and are never freed */
typedef struct Symbol Symbol;

typedef struct CommonSymbols {
  Symbol *init; /* __init__ */
  Symbol *repr; /* __repr__ */
} CommonSymbols;

u32 hashStringData(const u8 *key, size_t length);

Symbol *newSymbol(const char *s);
const char *symbolChars(Symbol *symbol);
size_t symbolByteLength(Symbol *symbol);
u32 symbolHash(Symbol *symbol);

const CommonSymbols *getCommonSymbols(void);

#endif /*mtots1symbol_h*/
