#ifndef mtots_debug_h
#define mtots_debug_h

#include "mtots_object.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif/*mtots_debug_h*/
