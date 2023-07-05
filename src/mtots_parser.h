#ifndef mtots_parser_h
#define mtots_parser_h

#include "mtots_object.h"
#include "mtots_lexer.h"

/* Compiles the given source into an ObjThunk */
ubool parse(const char *source, String *moduleName, ObjThunk **out);

void markParserRoots();

#endif/*mtots_parser_h*/
