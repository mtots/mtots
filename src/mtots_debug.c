#include "mtots_debug.h"

#include <stdio.h>

void disassembleChunk(Chunk *chunk, const char *name) {
  int offset;

  printf("== %s ==\n", name);

  for (offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static u16 readU16(u8 *code) {
  return (((u16)code[0]) << 8) | (u16)code[1];
}

static int constantInstruction(
    const char *name, Chunk *chunk, int offset) {
  u16 constant = readU16(chunk->code + (offset + 1));
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}

static int invokeInstruction(const char *name, Chunk *chunk, int offset) {
  u8 constant = chunk->code[offset + 1];
  u8 argCount = chunk->code[offset + 2];
  printf("%-16s (%d args) %4d '", name, argCount, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
  u8 slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(
    const char *name, int sign, Chunk *chunk, int offset) {
  u16 jump = (u16)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  u8 instruction;

  printf("%04d ", offset);
  if (offset > 0 &&
      chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  instruction = chunk->code[offset];
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_FIELD:
      return constantInstruction("OP_GET_FIELD", chunk, offset);
    case OP_SET_FIELD:
      return constantInstruction("OP_SET_FIELD", chunk, offset);
    case OP_IS:
      return simpleInstruction("OP_IS", offset);
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_FLOOR_DIVIDE:
      return simpleInstruction("OP_FLOOR_DIVIDE", offset);
    case OP_MODULO:
      return simpleInstruction("OP_MODULO", offset);
    case OP_POWER:
      return simpleInstruction("OP_POWER", offset);
    case OP_SHIFT_LEFT:
      return simpleInstruction("OP_SHIFT_LEFT", offset);
    case OP_SHIFT_RIGHT:
      return simpleInstruction("OP_SHIFT_RIGHT", offset);
    case OP_BITWISE_OR:
      return simpleInstruction("OP_BITWISE_OR", offset);
    case OP_BITWISE_AND:
      return simpleInstruction("OP_BITWISE_AND", offset);
    case OP_BITWISE_XOR:
      return simpleInstruction("OP_BITWISE_XOR", offset);
    case OP_BITWISE_NOT:
      return simpleInstruction("OP_BITWISE_NOT", offset);
    case OP_IN:
      return simpleInstruction("OP_IN", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_JUMP_IF_STOP_ITERATION:
      return jumpInstruction("OP_JUMP_IF_STOP_ITERATION", 1, chunk, offset);
    case OP_TRY_START:
      return jumpInstruction("OP_TRY_START", 1, chunk, offset);
    case OP_TRY_END:
      return jumpInstruction("OP_TRY_END", 1, chunk, offset);
    case OP_RAISE:
      return simpleInstruction("OP_RAISE", offset);
    case OP_GET_ITER:
      return simpleInstruction("OP_GET_ITER", offset);
    case OP_GET_NEXT:
      return simpleInstruction("OP_GET_NEXT", offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
    case OP_INVOKE:
      return invokeInstruction("OP_INVOKE", chunk, offset);
    case OP_SUPER_INVOKE:
      return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
    case OP_CLOSURE: {
      u8 constant;
      ObjThunk *thunk;
      i16 j;

      offset++;
      constant = chunk->code[offset++];
      printf("%-16s   %d ", "OP_CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");

      thunk = AS_THUNK(chunk->constants.values[constant]);
      for (j = 0; j < thunk->upvalueCount; j++) {
        i32 isLocal = chunk->code[offset++];
        i32 index = chunk->code[offset++];
        printf(
          "%04d    |                      %s %d\n",
          offset - 2, isLocal ? "local": "upvalue", index);
      }

      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_IMPORT:
      return constantInstruction("OP_IMPORT", chunk, offset);
    case OP_NEW_LIST:
      return byteInstruction("OP_NEW_LIST", chunk, offset);
    case OP_NEW_FROZEN_LIST:
      return byteInstruction("OP_NEW_FROZEN_LIST", chunk, offset);
    case OP_NEW_DICT:
      return byteInstruction("OP_NEW_DICT", chunk, offset);
    case OP_NEW_FROZEN_DICT:
      return byteInstruction("OP_NEW_FROZEN_DICT", chunk, offset);
    case OP_CLASS:
      return constantInstruction("OP_CLASS", chunk, offset);
    case OP_INHERIT:
      return simpleInstruction("OP_INHERIT", offset);
    case OP_METHOD:
      return constantInstruction("OP_METHOD", chunk, offset);
    case OP_STATIC_METHOD:
      return constantInstruction("OP_STATIC_METHOD", chunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
