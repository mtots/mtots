#include "mtots_m_zip.h"

#include "mtots_vm.h"

Value ZIP_ARCHIVE_VAL(ObjZipArchive *zip) {
  return OBJ_VAL_EXPLICIT((Obj*)zip);
}

static void freeZipArchive(ObjNative *n) {
  ObjZipArchive *za = (ObjZipArchive*)n;
  if (!mz_zip_reader_end(&za->handle)) {
    panic("miniz: %s", mz_zip_get_error_string(mz_zip_get_last_error(&za->handle)));
  }
}

NativeObjectDescriptor descriptorZipArchive = {
  nopBlacken, freeZipArchive, sizeof(ObjZipArchive), "ZipArchive",
};

static ubool minizErr(mz_zip_archive *za, const char *tag) {
  runtimeError("miniz %s: %s", tag, mz_zip_get_error_string(mz_zip_get_last_error(za)));
  return UFALSE;
}

ubool openZipArchiveFromFile(const char *path, ObjZipArchive **out) {
  ObjZipArchive *za = NEW_NATIVE(ObjZipArchive, &descriptorZipArchive);

  /*
   * Looking at the example here:
   * https://github.com/tessel/miniz/blob/master/example2.c
   *
   * zeroing out the struct seems to be required.
   *
   * When I don't zero out, I get an 'invalid parameter' error from miniz
   */
  memset(&za->handle, 0, sizeof(za->handle));

  if (!mz_zip_reader_init_file(&za->handle, path, 0)) {
    return minizErr(&za->handle, "mz_zip_reader_init_file");
  }
  *out = za;
  return UTRUE;
}

static ubool implZipArchiveStaticFromFile(i16 argc, Value *args, Value *out) {
  ObjZipArchive *za;
  String *path = AS_STRING(args[0]);
  if (!openZipArchiveFromFile(path->chars, &za)) {
    return UFALSE;
  }
  *out = ZIP_ARCHIVE_VAL(za);
  return UTRUE;
}

static CFunction funcZipArchiveStaticFromFile = {
  implZipArchiveStaticFromFile, "fromFile", 1, 0, argsStrings
};

static ubool implZipArchiveGetFileCount(i16 argc, Value *args, Value *out) {
  ObjZipArchive *za = AS_ZIP_ARCHIVE(args[-1]);
  *out = NUMBER_VAL(mz_zip_reader_get_num_files(&za->handle));
  return UTRUE;
}

static CFunction funcZipArchiveGetFileCount = { implZipArchiveGetFileCount, "getFileCount" };

static ubool implZipArchiveGetFileName(i16 argc, Value *args, Value *out) {
  ObjZipArchive *za = AS_ZIP_ARCHIVE(args[-1]);
  size_t index = AS_INDEX(args[0], mz_zip_reader_get_num_files(&za->handle));
  char *buffer;
  u32 fileNameSize;
  /* fileNameSize is inclusive of the null terminator */
  fileNameSize = mz_zip_reader_get_filename(&za->handle, (mz_uint)index, NULL, 0);
  buffer = (char*)malloc(fileNameSize);
  mz_zip_reader_get_filename(&za->handle, (mz_uint)index, buffer, fileNameSize);
  *out = STRING_VAL(internCString(buffer));
  free(buffer);
  return UTRUE;
}

static CFunction funcZipArchiveGetFileName = {
  implZipArchiveGetFileName, "getFileName", 1, 0, argsNumbers
};

static ubool implZipArchiveIsDirectory(i16 argc, Value *args, Value *out) {
  ObjZipArchive *za = AS_ZIP_ARCHIVE(args[-1]);
  size_t index = AS_INDEX(args[0], mz_zip_reader_get_num_files(&za->handle));
  *out = BOOL_VAL(!!mz_zip_reader_is_file_a_directory(&za->handle, (mz_uint)index));
  return UTRUE;
}

static CFunction funcZipArchiveIsDirectory = {
  implZipArchiveIsDirectory, "isDirectory", 1, 0, argsNumbers
};

static ubool implZipArchiveExtractToFile(i16 argc, Value *args, Value *out) {
  ObjZipArchive *za = AS_ZIP_ARCHIVE(args[-1]);
  size_t index = AS_INDEX(args[0], mz_zip_reader_get_num_files(&za->handle));
  String *path = AS_STRING(args[1]);
  if (!mz_zip_reader_extract_to_file(&za->handle, (mz_uint)index, path->chars, 0)) {
    return minizErr(&za->handle, "mz_zip_reader_extract_to_file");
  }
  return UTRUE;
}

static TypePattern argsZipArchiveExtractToFile[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_STRING },
};

static CFunction funcZipArchiveExtractToFile = {
  implZipArchiveExtractToFile, "extractToFile", 2, 0, argsZipArchiveExtractToFile
};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *staticZipMethods[] = {
    &funcZipArchiveStaticFromFile,
    NULL,
  };
  CFunction *zipMethods[] = {
    &funcZipArchiveGetFileCount,
    &funcZipArchiveGetFileName,
    &funcZipArchiveIsDirectory,
    &funcZipArchiveExtractToFile,
    NULL,
  };

  newNativeClass(module, &descriptorZipArchive, zipMethods, staticZipMethods);

  return UTRUE;
}

static CFunction func = { impl, "zip", 1 };

void addNativeModuleZip(void) {
  addNativeModule(&func);
}
