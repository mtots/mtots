#include "mtots_m_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_common.h"
#include "mtots_vm.h"

static Status implReadString(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  size_t fileSize;
  char *buffer;
  FILE *file;

  if (!getFileSize(fileName->chars, &fileSize)) {
    return STATUS_ERROR;
  }

  file = fopen(fileName->chars, "rb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return STATUS_ERROR;
  }

  buffer = (char *)malloc(fileSize);
  if (fread(buffer, 1, fileSize, file) != fileSize) {
    free(buffer);
    runtimeError("Error while reading file %s", fileName->chars);
    return STATUS_ERROR;
  }

  fclose(file);
  *out = valString(internString(buffer, fileSize));
  free(buffer);
  return STATUS_OK;
}

static CFunction funcReadString = {implReadString, "readString", 1, 0};

static Status implReadBytes(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  ObjBuffer *buffer;
  size_t fileSize;
  FILE *file;

  if (!getFileSize(fileName->chars, &fileSize)) {
    return STATUS_ERROR;
  }

  file = fopen(fileName->chars, "rb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return STATUS_ERROR;
  }

  buffer = newBuffer();
  push(valBuffer(buffer));
  bufferSetLength(&buffer->handle, fileSize);
  if (fread(buffer->handle.data, 1, fileSize, file) != fileSize) {
    runtimeError("Error while reading file %s", fileName->chars);
    return STATUS_ERROR;
  }

  fclose(file);
  *out = valBuffer(buffer);
  pop(); /* buffer */
  return STATUS_OK;
}

static CFunction funcReadBytes = {implReadBytes, "readBytes", 1, 0};

static Status implWriteString(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  String *data = asString(args[1]);
  FILE *file;

  file = fopen(fileName->chars, "wb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return STATUS_ERROR;
  }

  if (fwrite(data->chars, 1, data->byteLength, file) != data->byteLength) {
    runtimeError("Error while writing to file %s", fileName->chars);
    return STATUS_ERROR;
  }

  fclose(file);
  return STATUS_OK;
}

static CFunction funcWriteString = {implWriteString, "writeString", 2, 0};

static Status implWriteBytes(i16 argCount, Value *args, Value *out) {
  String *fileName = asString(args[0]);
  Buffer *data = &asBuffer(args[1])->handle;
  FILE *file;

  file = fopen(fileName->chars, "wb");
  if (!file) {
    runtimeError("Failed to open file %s", fileName->chars);
    return STATUS_ERROR;
  }

  if (fwrite(data->data, 1, data->length, file) != data->length) {
    runtimeError("Error while writing to file %s", fileName->chars);
    return STATUS_ERROR;
  }

  fclose(file);
  return STATUS_OK;
}

static CFunction funcWriteBytes = {implWriteBytes, "writeBytes", 2, 0};

static Status implIsFile(i16 argCount, Value *args, Value *out) {
  *out = valBool(isFile(asString(args[0])->chars));
  return STATUS_OK;
}

static CFunction funcIsFile = {implIsFile, "isFile", 1, 0};

static Status implIsDir(i16 argCount, Value *args, Value *out) {
  *out = valBool(isDirectory(asString(args[0])->chars));
  return STATUS_OK;
}

static CFunction funcIsDir = {implIsDir, "isDir", 1, 0};

static Status implListCallback(void *listPtr, const char *fileName) {
  ObjList *list = (ObjList *)listPtr;
  String *name;
  if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
    /* Exclude '.' and '..' */
    return STATUS_OK;
  }
  name = internCString(fileName);
  push(valString(name));
  listAppend(list, valString(name));
  pop(); /* name */
  return STATUS_OK;
}

static Status implList(i16 argCount, Value *args, Value *out) {
  const char *dirpath = argCount > 0 ? asString(args[0])->chars : ".";
  ObjList *entries;

  entries = newList(0);
  push(valList(entries));
  if (!listDirectory(dirpath, entries, implListCallback)) {
    return STATUS_ERROR;
  }
  *out = pop(); /* entries */
  return STATUS_OK;
}

static CFunction funcList = {implList, "list", 0, 1};

static Status implMkdir(i16 argc, Value *args, Value *out) {
  const char *dirpath = asString(args[0])->chars;
  ubool existOk = argc > 1 ? asBool(args[1]) : UFALSE; /* like `-p` in `mkdir -p` */
  return makeDirectory(dirpath, existOk);
}

static CFunction funcMkdir = {implMkdir, "mkdir", 1, 2};

/* TODO: For now, this is basically just a simple join, but in
 * the future, this function will have to evolve to match the
 * behaviors you would expect from a path join */
static Status implJoin(i16 argCount, Value *args, Value *out) {
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

  *out = valString(bufferToString(&buf));

  freeBuffer(&buf);

  return STATUS_OK;
}

static CFunction funcJoin = {implJoin, "join", 1, 0};

static Status implDirname(i16 argCount, Value *args, Value *out) {
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
  *out = valString(internString(chars, i));
  return STATUS_OK;
}

static CFunction funcDirname = {implDirname, "dirname", 1, 0};

static Status implBasename(i16 argCount, Value *args, Value *out) {
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
  *out = valString(internString(chars + i, end - i));
  return STATUS_OK;
}

static CFunction funcBasename = {implBasename, "basename", 1, 0};

static Status impl(i16 argCount, Value *args, Value *out) {
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
  mapSetN(&module->fields, "sep", valString(internCString(PATH_SEP_STR)));

  return STATUS_OK;
}

static CFunction func = {impl, "fs", 1};

void addNativeModuleFs(void) {
  addNativeModule(&func);
}
