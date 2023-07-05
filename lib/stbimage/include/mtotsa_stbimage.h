#ifndef mtotsa_stbimage_h
#define mtotsa_stbimage_h

int mtotsa_load_image_from_memory(
  const unsigned char *buffer,
  int len,
  unsigned char **data,
  int *width,
  int *height);

int mtotsa_load_image_from_file(
  const char *filePath,
  unsigned char **data,
  int *width,
  int *height);

void mtotsa_free_image_data(unsigned char *data);

#endif/*mtotsa_stbimage_h*/
