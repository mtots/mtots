#ifndef mtots1symbol_h
#define mtots1symbol_h

#include "mtots0common.h"

/** Symbols are interned strings that can be easily check for equality
 * and are never freed */
typedef struct Symbol Symbol;

typedef struct CommonSymbols {
  Symbol *dunderInit; /* __init__ */
  Symbol *dunderRepr; /* __repr__ */
  Symbol *dunderCall; /* __call__ */
  Symbol *dunderMain; /* __main__ */
} CommonSymbols;

u32 hashStringData(const u8 *key, size_t length);

Symbol *newSymbol(const char *s);
Symbol *newSymbolWithLength(const char *s, size_t byteLength);
const char *symbolChars(Symbol *symbol);
size_t symbolByteLength(Symbol *symbol);
u32 symbolHash(Symbol *symbol);

const CommonSymbols *getCommonSymbols(void);

#endif /*mtots1symbol_h*/
