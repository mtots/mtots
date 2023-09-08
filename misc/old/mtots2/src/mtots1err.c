#include "mtots1err.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define ERROR_BUFFER_SIZE 1024
#define ERROR_FRAME_COUNT 1024

typedef struct ErrorFrame {
  const char *message;
  Symbol *fileName;
  u32 line;
} ErrorFrame;

static char errorString[ERROR_BUFFER_SIZE];
static ErrorFrame errorFrames[ERROR_FRAME_COUNT];
static ErrorFrameHandle errorFrameCount;

NORETURN void panic(const char *format, ...) {
  ErrorFrameHandle i;
  va_list args;
  va_start(args, format);
  fflush(stdout);
  for (i = 0; i < errorFrameCount; i++) {
    ErrorFrame *frame = &errorFrames[i];
    fprintf(stderr, " ");
    if (frame->message) {
      fprintf(stderr, " %s", frame->message);
    }
    if (frame->fileName) {
      fprintf(stderr, " in file \"%s\"", symbolChars(frame->fileName));
    }
    if (frame->line) {
      fprintf(stderr, " on line %lu", (unsigned long)frame->line);
    }
    fprintf(stderr, "\n");
  }
  fputs("PANIC: ", stderr);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end(args);
  abort();
}

void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(errorString, ERROR_BUFFER_SIZE, format, args);
  va_end(args);
}

const char *getErrorString(void) {
  return errorString[0] == '\0' ? NULL : errorString;
}

ErrorFrameHandle pushErrorFrame(const char *message, Symbol *fileName, u32 line) {
  ErrorFrame *frame;
  ErrorFrameHandle current = errorFrameCount;
  if (errorFrameCount >= ERROR_FRAME_COUNT) {
    panic("stackoverflow (pushErrorFrame)");
  }
  frame = &errorFrames[errorFrameCount++];
  frame->message = message;
  frame->fileName = fileName;
  frame->line = line;
  return current;
}

void popErrorFrame(ErrorFrameHandle newFrameCount) {
  if (newFrameCount >= errorFrameCount) {
    panic("popErrorFrame - pop to position higher than or equal to current");
  }
  errorFrameCount = newFrameCount;
}
