#ifndef mtots_chunk_h
#define mtots_chunk_h

#include "mtots_value.h"

typedef enum OpCode {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_FIELD,
  OP_SET_FIELD,
  OP_IS,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_FLOOR_DIVIDE,
  OP_MODULO,
  OP_POWER,
  OP_SHIFT_LEFT,
  OP_SHIFT_RIGHT,
  OP_BITWISE_OR,
  OP_BITWISE_AND,
  OP_BITWISE_XOR,
  OP_BITWISE_NOT,
  OP_IN,
  OP_NOT,
  OP_NEGATE,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_JUMP_IF_STOP_ITERATION,
  OP_TRY_START,
  OP_TRY_END,
  OP_RAISE,
  OP_GET_ITER, /* iterable TOS, replaces with iterator */
  OP_GET_NEXT, /* iterator TOS, pushes next item */
  OP_LOOP,
  OP_CALL,
  OP_INVOKE,
  OP_SUPER_INVOKE,
  OP_CALL_KW,
  OP_INVOKE_KW,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
  OP_IMPORT,
  OP_NEW_LIST,
  OP_NEW_FROZEN_LIST,
  OP_NEW_DICT,
  OP_NEW_FROZEN_DICT,
  OP_CLASS,
  OP_INHERIT,
  OP_METHOD,
  OP_STATIC_METHOD
} OpCode;

typedef struct Chunk {
  i32 count;
  i32 capacity;
  u8 *code;
  i16 *lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, u8 byte, i16 line);
size_t addConstant(Chunk *chunk, Value value);

#endif /*mtots_chunk_h*/
