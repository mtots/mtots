#ifndef mtots_util_symbol_h
#define mtots_util_symbol_h

#include "mtots_common.h"

typedef struct Symbol Symbol;

Symbol *internStringWithLength(const char *chars, size_t length);
Symbol *internSymbol(const char *cstring);
const char *getSymbolChars(Symbol *symbol);
size_t getSymbolByteLength(Symbol *symbol);
u32 getSymbolHash(Symbol *symbol);

#endif /*mtots_util_symbol_h*/
