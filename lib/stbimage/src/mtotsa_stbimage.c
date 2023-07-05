#include "mtotsa_stbimage.h"


#define STB_IMAGE_STATIC
#define STBI_FAILURE_USERMSG
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef __GNUC__
#define MTOTS_PRINTFLIKE(n,m) __attribute__((format(printf,n,m)))
#else
#define MTOTS_PRINTFLIKE(n,m)
#endif /* __GNUC__ */


extern void runtimeError(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

/* Load an image from memory.
 * Returns 0 on error.
 * Calls 'runtimeError(const char*, ...)' on error */
int mtotsa_load_image_from_memory(
    const unsigned char *buffer,
    int len,
    unsigned char **data,
    int *width,
    int *height) {
  *data = stbi_load_from_memory(buffer, len, width, height, NULL, 4);
  if (!*data) {
    runtimeError("stbi_load_from_memory: %s", stbi_failure_reason());
    return 0;
  }
  return 1;
}

int mtotsa_load_image_from_file(
    const char *filePath,
    unsigned char **data,
    int *width,
    int *height) {
  *data = stbi_load(filePath, width, height, NULL, 4);
  if (!*data) {
    runtimeError("stbi_load_from_memory: %s", stbi_failure_reason());
    return 0;
  }
  return 1;
}

void mtotsa_free_image_data(unsigned char *data) {
  stbi_image_free(data);
}
