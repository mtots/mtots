#ifndef mtots_m_data_h
#define mtots_m_data_h

#include "mtots_object.h"

/* Native Module data
 * For reading and writing data from and to various
 * sources and sinks */

#define IS_DATA_SOURCE(v) (getNativeObjectDescriptor(v) == &descriptorDataSource)
#define IS_DATA_SINK(v) (getNativeObjectDescriptor(v) == &descriptorDataSink)
#define AS_DATA_SOURCE(v) ((ObjDataSource*)AS_OBJ(v))
#define AS_DATA_SINK(v) ((ObjDataSink*)AS_OBJ(v))

typedef enum DataSourceType {
  DATA_SOURCE_BUFFER,
  DATA_SOURCE_STRING,
  DATA_SOURCE_FILE,
  DATA_SOURCE_BUNDLE
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
    struct {
      String *src;
      String *path;
    } bundle;
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

Value DATA_SOURCE_VAL(ObjDataSource *ds);
Value DATA_SINK_VAL(ObjDataSink *ds);

ObjDataSource *newDataSourceFromBuffer(ObjBuffer *buffer);
ObjDataSource *newDataSourceFromString(String *string);
ObjDataSource *newDataSourceFromFile(String *filePath);
ObjDataSource *newDataSourceFromBundle(String *src, String *path);
ubool dataSourceInitBuffer(ObjDataSource *ds, Buffer *out);
ubool dataSourceReadIntoBuffer(ObjDataSource *ds, Buffer *out);
ubool dataSourceReadToString(ObjDataSource *ds, String **out);

ObjDataSink *newDataSinkFromBuffer(ObjBuffer *buffer);
ObjDataSink *newDataSinkFromFile(String *filePath);
ubool dataSinkWriteBytes(ObjDataSink *ds, const u8 *data, size_t dataLen);
ubool dataSinkWrite(ObjDataSink *sink, ObjDataSource *src);

void addNativeModuleData(void);

#endif/*mtots_m_data_h*/
