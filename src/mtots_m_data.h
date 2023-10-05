#ifndef mtots_m_data_h
#define mtots_m_data_h

#include "mtots_object.h"

/* Native Module data
 * For reading and writing data from and to various
 * sources and sinks */

#define isDataSource(v) (getNativeObjectDescriptor(v) == &descriptorDataSource)
#define isDataSink(v) (getNativeObjectDescriptor(v) == &descriptorDataSink)

typedef enum DataSourceType {
  DATA_SOURCE_BUFFER,
  DATA_SOURCE_STRING,
  DATA_SOURCE_FILE
} DataSourceType;

typedef struct ObjDataSource {
  ObjNative obj;
  DataSourceType type;
  union {
    ObjBuffer *buffer;
    String *string;
    struct {
      String *path;
    } file;
  } as;
} ObjDataSource;

typedef enum DataSinkType {
  DATA_SINK_BUFFER,
  DATA_SINK_FILE
} DataSinkType;

typedef struct ObjDataSink {
  ObjNative obj;
  DataSinkType type;
  union {
    ObjBuffer *buffer;
    struct {
      String *path;
    } file;
  } as;
} ObjDataSink;

extern NativeObjectDescriptor descriptorDataSource;
extern NativeObjectDescriptor descriptorDataSink;

Value valDataSource(ObjDataSource *ds);
Value valDataSink(ObjDataSink *ds);

ObjDataSource *asDataSource(Value value);
ObjDataSink *asDataSink(Value value);

ObjDataSource *newDataSourceFromBuffer(ObjBuffer *buffer);
ObjDataSource *newDataSourceFromString(String *string);
ObjDataSource *newDataSourceFromFile(String *filePath);
ubool dataSourceInitBuffer(ObjDataSource *ds, Buffer *out);
ubool dataSourceReadIntoBuffer(ObjDataSource *ds, Buffer *out);
ubool dataSourceReadToString(ObjDataSource *ds, String **out);

ObjDataSink *newDataSinkFromBuffer(ObjBuffer *buffer);
ObjDataSink *newDataSinkFromFile(String *filePath);
ubool dataSinkWriteBytes(ObjDataSink *ds, const u8 *data, size_t dataLen);
ubool dataSinkWrite(ObjDataSink *sink, ObjDataSource *src);

void addNativeModuleData(void);

#endif /*mtots_m_data_h*/
