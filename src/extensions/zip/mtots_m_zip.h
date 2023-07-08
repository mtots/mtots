#ifndef mtots_m_zip_h
#define mtots_m_zip_h

/* Native Module zip */

#include "mtots_object.h"

#include "miniz.h"

#define AS_ZIP_ARCHIVE(value) ((ObjZipArchive*)AS_OBJ(value))
#define IS_ZIP_ARCHIVE(value) (getNativeObjectDescriptor(value) == &descriptorZipArchive)

typedef struct ObjZipArchive {
  ObjNative obj;
  mz_zip_archive handle;
} ObjZipArchive;

Value ZIP_ARCHIVE_VAL(ObjZipArchive *zip);

ubool openZipArchiveFromFile(const char *path, ObjZipArchive **out);
void addNativeModuleZip(void);

extern NativeObjectDescriptor descriptorZipArchive;

#endif/*mtots_m_zip_h*/
