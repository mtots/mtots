#include "mtots_m_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_common.h"
#include "mtots_vm.h"

static ubool implReadString(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  size_t fileSize;
  char *buffer;
  FILE *file;

  if (!getFileSize(fileName->chars, &fileSize)) {
    return UFALSE;
  }

  file = fopen(fileName->chars, "rb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return UFALSE;
  }

  buffer = (char *)malloc(fileSize);
  if (fread(buffer, 1, fileSize, file) != fileSize) {
    free(buffer);
    runtimeError("Error while reading file %s", fileName->chars);
    return UFALSE;
  }

  fclose(file);
  *out = STRING_VAL(internString(buffer, fileSize));
  free(buffer);
  return UTRUE;
}

static CFunction funcReadString = {implReadString, "readString", 1, 0};

static ubool implReadBytes(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  ObjBuffer *buffer;
  size_t fileSize;
  FILE *file;

  if (!getFileSize(fileName->chars, &fileSize)) {
    return UFALSE;
  }

  file = fopen(fileName->chars, "rb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return UFALSE;
  }

  buffer = newBuffer();
  push(BUFFER_VAL(buffer));
  bufferSetLength(&buffer->handle, fileSize);
  if (fread(buffer->handle.data, 1, fileSize, file) != fileSize) {
    runtimeError("Error while reading file %s", fileName->chars);
    return UFALSE;
  }

  fclose(file);
  *out = BUFFER_VAL(buffer);
  pop(); /* buffer */
  return UTRUE;
}

static CFunction funcReadBytes = {implReadBytes, "readBytes", 1, 0};

static ubool implWriteString(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  String *data = asString(args[1]);
  FILE *file;

  file = fopen(fileName->chars, "wb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return UFALSE;
  }

  if (fwrite(data->chars, 1, data->byteLength, file) != data->byteLength) {
    runtimeError("Error while writing to file %s", fileName->chars);
    return UFALSE;
  }

  fclose(file);
  return UTRUE;
}

static CFunction funcWriteString = {implWriteString, "writeString", 2, 0};

static ubool implWriteBytes(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  Buffer *data = &asBuffer(args[1])->handle;
  FILE *file;

  file = fopen(fileName->chars, "wb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return UFALSE;
  }

  if (fwrite(data->data, 1, data->length, file) != data->length) {
    runtimeError("Error while writing to file %s", fileName->chars);
    return UFALSE;
  }

  fclose(file);
  return UTRUE;
}

static CFunction funcWriteBytes = {implWriteBytes, "writeBytes", 2, 0};

static ubool implIsFile(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(isFile(asString(args[0])->chars));
  return UTRUE;
}

static CFunction funcIsFile = {implIsFile, "isFile", 1, 0};

static ubool implIsDir(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(isDirectory(asString(args[0])->chars));
  return UTRUE;
}

static CFunction funcIsDir = {implIsDir, "isDir", 1, 0};

static ubool implListCallback(void *listPtr, const char *fileName) {
  ObjList *list = (ObjList *)listPtr;
  String *name;
  if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
    /* Exclude '.' and '..' */
    return UTRUE;
  }
  name = internCString(fileName);
  push(STRING_VAL(name));
  listAppend(list, STRING_VAL(name));
  pop(); /* name */
  return UTRUE;
}

static ubool implList(i16 argCount, Value *args, Value *out) {
  const char *dirpath = argCount > 0 ? asString(args[0])->chars : ".";
  ObjList *entries;

  entries = newList(0);
  push(LIST_VAL(entries));
  if (!listDirectory(dirpath, entries, implListCallback)) {
    return UFALSE;
  }
  *out = pop(); /* entries */
  return UTRUE;
}

static CFunction funcList = {implList, "list", 0, 1};

static ubool implMkdir(i16 argc, Value *args, Value *out) {
  const char *dirpath = asString(args[0])->chars;
  ubool existOk = argc > 1 ? asBool(args[1]) : UFALSE; /* like `-p` in `mkdir -p` */
  return makeDirectory(dirpath, existOk);
}

static CFunction funcMkdir = {implMkdir, "mkdir", 1, 2};

/* TODO: For now, this is basically just a simple join, but in
 * the future, this function will have to evolve to match the
 * behaviors you would expect from a path join */
static ubool implJoin(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[0]);
  size_t i, len = list->length;
  Buffer buf;

  initBuffer(&buf);

  for (i = 0; i < len; i++) {
    String *item;
    if (i > 0) {
      bputchar(&buf, PATH_SEP);
    }
    item = asString(list->buffer[i]);
    bputstrlen(&buf, item->chars, item->byteLength);
  }

  *out = STRING_VAL(bufferToString(&buf));

  freeBuffer(&buf);

  return UTRUE;
}

static CFunction funcJoin = {implJoin, "join", 1, 0};

static ubool implDirname(i16 argCount, Value *args, Value *out) {
  String *string = asString(args[0]);
  const char *chars = string->chars;
  size_t i = string->byteLength;
  while (i > 0 && chars[i - 1] == PATH_SEP) {
    i--;
  }
  for (; i > 0; i--) {
    if (chars[i - 1] == PATH_SEP) {
      i--;
      break;
    }
  }
  *out = STRING_VAL(internString(chars, i));
  return UTRUE;
}

static CFunction funcDirname = {implDirname, "dirname", 1, 0};

static ubool implBasename(i16 argCount, Value *args, Value *out) {
  String *string = asString(args[0]);
  const char *chars = string->chars;
  size_t i = string->byteLength, end;
  while (i > 0 && chars[i - 1] == PATH_SEP) {
    i--;
  }
  end = i;
  for (; i > 0; i--) {
    if (chars[i - 1] == PATH_SEP) {
      break;
    }
  }
  *out = STRING_VAL(internString(chars + i, end - i));
  return UTRUE;
}

static CFunction funcBasename = {implBasename, "basename", 1, 0};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcReadString,
      &funcReadBytes,
      &funcWriteString,
      &funcWriteBytes,
      &funcIsFile,
      &funcIsDir,
      &funcList,
      &funcMkdir,
      &funcJoin,
      &funcDirname,
      &funcBasename,
      NULL,
  };

  moduleAddFunctions(module, functions);
  mapSetN(&module->fields, "sep", STRING_VAL(internCString(PATH_SEP_STR)));

  return UTRUE;
}

static CFunction func = {impl, "fs", 1};

void addNativeModuleFs(void) {
  addNativeModule(&func);
}
