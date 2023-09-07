#include <stdlib.h>

#include "mtots_vm.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(u8, chunk->code, chunk->capacity);
  FREE_ARRAY(i16, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

void writeChunk(Chunk *chunk, u8 byte, i16 line) {
  if (chunk->capacity < chunk->count + 1) {
    i32 oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(
        u8, chunk->code, oldCapacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(
        i16, chunk->lines, oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

size_t addConstant(Chunk *chunk, Value value) {
  size_t i;

  /* If we find the same constant already in the constants array,
   * just use that instead */
  for (i = 0; i < chunk->constants.count; i++) {
    if (valuesIs(value, chunk->constants.values[i])) {
      return i;
    }
  }

  push(value);
  writeValueArray(&chunk->constants, value);
  pop();
  return chunk->constants.count - 1;
}
