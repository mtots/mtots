#include "mtots_m_data.h"

#include "mtots_bundle.h"
#include "mtots_vm.h"

static void blackenDataSource(ObjNative *n) {
  ObjDataSource *dataSource = (ObjDataSource*)n;
  switch (dataSource->type) {
    case DATA_SOURCE_BUFFER:
      markObject((Obj*)dataSource->as.buffer);
      break;
    case DATA_SOURCE_STRING:
      markString(dataSource->as.string);
      break;
    case DATA_SOURCE_FILE:
      markString(dataSource->as.file.path);
      break;
    case DATA_SOURCE_BUNDLE:
      markString(dataSource->as.bundle.src);
      markString(dataSource->as.bundle.path);
      break;
    default:
      panic("Invalid DataSourceType %d", dataSource->type);
  }
}

static void blackenDataSink(ObjNative *n) {
  ObjDataSink *dataSink = (ObjDataSink*)n;
  switch (dataSink->type) {
    case DATA_SINK_BUFFER:
      markObject((Obj*)dataSink->as.buffer);
      break;
    case DATA_SINK_FILE:
      markString(dataSink->as.file.path);
      break;
    default:
      panic("Invalid DataSinkType %d", dataSink->type);
  }
}

NativeObjectDescriptor descriptorDataSource = {
  blackenDataSource, nopFree,
  sizeof(ObjDataSource), "DataSource",
};

NativeObjectDescriptor descriptorDataSink = {
  blackenDataSink, nopFree,
  sizeof(ObjDataSink), "DataSink",
};

Value DATA_SOURCE_VAL(ObjDataSource *ds) {
  return OBJ_VAL_EXPLICIT((Obj*)ds);
}

Value DATA_SINK_VAL(ObjDataSink *ds) {
  return OBJ_VAL_EXPLICIT((Obj*)ds);
}

ObjDataSource *newDataSourceFromBuffer(ObjBuffer *buffer) {
  ObjDataSource *ds = NEW_NATIVE(ObjDataSource, &descriptorDataSource);
  ds->type = DATA_SOURCE_BUFFER;
  ds->as.buffer = buffer;
  return ds;
}

ObjDataSource *newDataSourceFromString(String *string) {
  ObjDataSource *ds = NEW_NATIVE(ObjDataSource, &descriptorDataSource);
  ds->type = DATA_SOURCE_STRING;
  ds->as.string = string;
  return ds;
}

ObjDataSource *newDataSourceFromFile(String *filePath) {
  ObjDataSource *ds = NEW_NATIVE(ObjDataSource, &descriptorDataSource);
  ds->type = DATA_SOURCE_FILE;
  ds->as.file.path = filePath;
  return ds;
}

ObjDataSource *newDataSourceFromBundle(String *src, String *path) {
  ObjDataSource *ds = NEW_NATIVE(ObjDataSource, &descriptorDataSource);
  ds->type = DATA_SOURCE_BUNDLE;
  ds->as.bundle.src = src;
  ds->as.bundle.path = path;
  return ds;
}

/*
 * Like 'dataSourceReadIntoBuffer', but
 *   - initializes the out Buffer (so the Buffer argument should be uninitialized), and
 *   - when possible, shares the raw data with the ObjDataSource.
 *     When the data source is not in memory, 'dataSourceReadIntoBuffer'
 *     is called to load the data.
 *
 * NOTE: If this function errors, you should assume that 'out' has NOT initialized,
 * so it is not necessary (or appropriate) to call 'freeBuffer' on it.
 */
ubool dataSourceInitBuffer(ObjDataSource *ds, Buffer *out) {
  switch (ds->type) {
    case DATA_SOURCE_BUFFER:
      initBufferWithExternalData(
        out, ds->as.buffer->handle.data, ds->as.buffer->handle.length);
      return UTRUE;
    default:break;
  }
  initBuffer(out);
  if (!dataSourceReadIntoBuffer(ds, out)) {
    freeBuffer(out);
    return UFALSE;
  }
  return UTRUE;
}

ubool dataSourceReadIntoBuffer(ObjDataSource *ds, Buffer *out) {
  switch (ds->type) {
    case DATA_SOURCE_BUFFER:
      bufferAddBytes(out, ds->as.buffer->handle.data, ds->as.buffer->handle.length);
      return UTRUE;
    case DATA_SOURCE_STRING:
      bufferAddBytes(out, ds->as.string->chars, ds->as.string->byteLength);
      return UTRUE;
    case DATA_SOURCE_FILE:
      return readFileIntoBuffer(ds->as.file.path->chars, out);
    case DATA_SOURCE_BUNDLE:
      return readBufferFromBundle(ds->as.bundle.src->chars, ds->as.bundle.path->chars, out);
    default:break;
  }
  panic("Invalid DataSourceType %d", ds->type);
  return UFALSE; /* unreachable */
}

ubool dataSourceReadToString(ObjDataSource *ds, String **out) {
  switch (ds->type) {
    case DATA_SOURCE_BUFFER:
      *out = internString((char*)ds->as.buffer->handle.data, ds->as.buffer->handle.length);
      return UTRUE;
    case DATA_SOURCE_STRING:
      *out = ds->as.string;
      return UTRUE;
    case DATA_SOURCE_BUNDLE:
      return readStringFromBundle(ds->as.bundle.src->chars, ds->as.bundle.path->chars, out);
    default: break;
  }

  /* If not one of the cases enumerated above, we fall back to reading
   * into a Buffer */
  {
    Buffer buffer;
    initBuffer(&buffer);
    if (!dataSourceReadIntoBuffer(ds, &buffer)) {
      freeBuffer(&buffer);
      return UFALSE;
    }
    *out = internString((char*)buffer.data, buffer.length);
    freeBuffer(&buffer);
  }
  return UTRUE;
}

ObjDataSink *newDataSinkFromBuffer(ObjBuffer *buffer) {
  ObjDataSink *ds = NEW_NATIVE(ObjDataSink, &descriptorDataSink);
  ds->type = DATA_SINK_BUFFER;
  ds->as.buffer = buffer;
  return ds;
}

ObjDataSink *newDataSinkFromFile(String *filePath) {
  ObjDataSink *ds = NEW_NATIVE(ObjDataSink, &descriptorDataSink);
  ds->type = DATA_SINK_FILE;
  ds->as.file.path = filePath;
  return ds;
}

ubool dataSinkWriteBytes(ObjDataSink *ds, const u8 *data, size_t dataLen) {
  switch (ds->type) {
    case DATA_SINK_BUFFER:
      bufferAddBytes(&ds->as.buffer->handle, (const void*)data, dataLen);
      return UTRUE;
    case DATA_SINK_FILE:
      return writeFile((void*)data, dataLen, ds->as.file.path->chars);
    default: break;
  }
  runtimeError("dataSinkWriteBytes: invalid data sink type %d", ds->type);
  return UFALSE;
}

ubool dataSinkWrite(ObjDataSink *sink, ObjDataSource *src) {
  switch (src->type) {
    case DATA_SOURCE_BUFFER:
      return dataSinkWriteBytes(
        sink,
        src->as.buffer->handle.data,
        src->as.buffer->handle.length);
    case DATA_SOURCE_STRING:
      return dataSinkWriteBytes(
        sink,
        (const u8*)src->as.string->chars,
        src->as.string->byteLength);
    default:break;
  }
  /* Fallback - use generic methods and store to an intermediate buffer */
  {
    Buffer buffer;
    if (!dataSourceInitBuffer(src, &buffer)) {
      return UFALSE;
    }
    if (!dataSinkWriteBytes(sink, buffer.data, buffer.length)) {
      freeBuffer(&buffer);
      return UFALSE;
    }
    freeBuffer(&buffer);
  }
  return UTRUE;
}

static ubool implFromBuffer(i16 argc, Value *args, Value *out) {
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  *out = DATA_SOURCE_VAL(newDataSourceFromBuffer(buffer));
  return UTRUE;
}

static TypePattern argsFromBuffer[] = {
  { TYPE_PATTERN_BUFFER },
};

static CFunction funcFromBuffer = {
  implFromBuffer, "fromBuffer", 1, 0, argsFromBuffer
};

static ubool implFromString(i16 argc, Value *args, Value *out) {
  String *path = AS_STRING(args[0]);
  *out = DATA_SOURCE_VAL(newDataSourceFromString(path));
  return UTRUE;
}

static CFunction funcFromString = {
  implFromString, "fromString", 1, 0, argsStrings
};

static ubool implFromFile(i16 argc, Value *args, Value *out) {
  String *path = AS_STRING(args[0]);
  *out = DATA_SOURCE_VAL(newDataSourceFromFile(path));
  return UTRUE;
}

static CFunction funcFromFile = {
  implFromFile, "fromFile", 1, 0, argsStrings
};

static ubool implFromBundle(i16 argc, Value *args, Value *out) {
  String *src = AS_STRING(args[0]);
  String *path = AS_STRING(args[1]);
  *out = DATA_SOURCE_VAL(newDataSourceFromBundle(src, path));
  return UTRUE;
}

static CFunction funcFromBundle = {
  implFromBundle, "fromBundle", 2, 0, argsStrings
};

static ubool implToBuffer(i16 argc, Value *args, Value *out) {
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  *out = DATA_SINK_VAL(newDataSinkFromBuffer(buffer));
  return UTRUE;
}

static TypePattern argsToBuffer[] = {
  { TYPE_PATTERN_BUFFER },
};

static CFunction funcToBuffer = {
  implToBuffer, "toBuffer", 1, 0, argsToBuffer
};

static CFunction funcDataSinkStaticFromBuffer = {
  implToBuffer, "fromBuffer", 1, 0, argsToBuffer
};

static ubool implToFile(i16 argc, Value *args, Value *out) {
  String *filePath = AS_STRING(args[0]);
  *out = DATA_SINK_VAL(newDataSinkFromFile(filePath));
  return UTRUE;
}

static CFunction funcToFile = {
  implToFile, "toFile", 1, 0, argsStrings
};

static CFunction funcDataSinkStaticFromFile = {
  implToFile, "fromFile", 1, 0, argsStrings
};

static ubool implDataSourceRead(i16 argc, Value *args, Value *out) {
  ObjDataSource *ds = AS_DATA_SOURCE(args[-1]);
  ObjBuffer *buf = AS_BUFFER(args[0]);
  return dataSourceReadIntoBuffer(ds, &buf->handle);
}

static TypePattern argsDataSourceRead[] = {
  { TYPE_PATTERN_BUFFER },
};

static CFunction funcDataSourceRead = {
  implDataSourceRead, "read", 1, 0, argsDataSourceRead
};

static ubool implDataSourceToBuffer(i16 argc, Value *args, Value *out) {
  ObjDataSource *ds = AS_DATA_SOURCE(args[-1]);
  ObjBuffer *buf = newBuffer();
  ubool gcPause;

  LOCAL_GC_PAUSE(gcPause);

  if (!dataSourceReadIntoBuffer(ds, &buf->handle)) {
    LOCAL_GC_UNPAUSE(gcPause);
    return UFALSE;
  }

  LOCAL_GC_UNPAUSE(gcPause);

  *out = BUFFER_VAL(buf);
  return UTRUE;
}

static CFunction funcDataSourceToBuffer = { implDataSourceToBuffer, "toBuffer" };

static ubool implDataSourceToString(i16 argc, Value *args, Value *out) {
  ObjDataSource *ds = AS_DATA_SOURCE(args[-1]);
  String *string;

  if (!dataSourceReadToString(ds, &string)) {
    return UFALSE;
  }

  *out = STRING_VAL(string);
  return UTRUE;
}

static CFunction funcDataSourceToString = { implDataSourceToString, "toString" };

static ubool implDataSinkWrite(i16 argc, Value *args, Value *out) {
  ObjDataSink *sink = AS_DATA_SINK(args[-1]);
  ObjDataSource *src = AS_DATA_SOURCE(args[0]);
  return dataSinkWrite(sink, src);
}

static TypePattern argsDataSinkWrite[] = {
  { TYPE_PATTERN_NATIVE, &descriptorDataSource },
};

static CFunction funcDataSinkWrite = {
  implDataSinkWrite, "write", 1, 0, argsDataSinkWrite
};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
    &funcFromBuffer,
    &funcFromString,
    &funcFromFile,
    &funcFromBundle,
    &funcToBuffer,
    &funcToFile,
    NULL,
  };
  CFunction *dataSourceStaticMethods[] = {
    &funcFromBuffer,
    &funcFromString,
    &funcFromFile,
    &funcFromBundle,
    NULL,
  };
  CFunction *dataSourceMethods[] = {
    &funcDataSourceRead,
    &funcDataSourceToBuffer,
    &funcDataSourceToString,
    NULL,
  };
  CFunction *dataSinkStaticMethods[] = {
    &funcDataSinkStaticFromBuffer,
    &funcDataSinkStaticFromFile,
    NULL,
  };
  CFunction *dataSinkMethods[] = {
    &funcDataSinkWrite,
    NULL,
  };

  moduleAddFunctions(module, functions);
  newNativeClass(
    module,
    &descriptorDataSource,
    dataSourceMethods,
    dataSourceStaticMethods);
  newNativeClass(
    module,
    &descriptorDataSink,
    dataSinkMethods,
    dataSinkStaticMethods);

  return UTRUE;
}

static CFunction func = { impl, "data", 1 };

void addNativeModuleData() {
  addNativeModule(&func);
}
