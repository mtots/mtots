#ifndef mtots_m_paco_ss_h
#define mtots_m_paco_ss_h

#include "mtots_vm.h"
#include "mtots_m_paco_common.h"
#include <stdlib.h>
#include <string.h>

typedef struct ObjSpriteSheet {
  ObjNative obj;
  u8 *pixels;
  size_t width, height;
  size_t spriteWidth, spriteHeight;
} ObjSpriteSheet;

static void freeSpriteSheet(ObjNative *n) {
  ObjSpriteSheet *sheet = (ObjSpriteSheet*)n;
  FREE_ARRAY(u8, sheet->pixels, sheet->width * sheet->height);
}

static NativeObjectDescriptor descriptorSpriteSheet = {
  nopBlacken, freeSpriteSheet,
  sizeof(ObjSpriteSheet), "SpriteSheet",
};

static ubool newSpriteSheet(
    size_t width, size_t height, size_t spriteWidth, size_t spriteHeight,
    ObjSpriteSheet **out) {
  ObjSpriteSheet *sheet;

  if (width % 4 != 0) {
    runtimeError(
      "sprite sheet width must be a multiple of 4 (got %lu)",
      (unsigned long)width);
    return UFALSE;
  }

  if (width % spriteWidth != 0) {
    runtimeError(
      "spriteWidth (%lu) does not evenly divide into sheet width (%lu)",
      (unsigned long)spriteWidth,
      (unsigned long)width);
    return UFALSE;
  }

  if (height % spriteHeight != 0) {
    runtimeError(
      "spriteHeight (%lu) does not evenly divide into sheet height (%lu)",
      (unsigned long)spriteHeight,
      (unsigned long)height);
    return UFALSE;
  }

  sheet = NEW_NATIVE(ObjSpriteSheet, &descriptorSpriteSheet);
  push(OBJ_VAL_EXPLICIT((Obj*)sheet));
  sheet->pixels = ALLOCATE(u8, width * height);
  sheet->width = width;
  sheet->height = height;
  sheet->spriteWidth = spriteWidth;
  sheet->spriteHeight = spriteHeight;
  memset(sheet->pixels, 0, width * height);
  *out = sheet;
  pop(); /* sheet */
  return UTRUE;
}

static ubool implSpriteSheetClone(i16 argCount, Value *args, Value *out) {
  ObjSpriteSheet *original = (ObjSpriteSheet*)AS_OBJ(args[-1]);
  ObjSpriteSheet *clone;
  if (!newSpriteSheet(
      original->width,
      original->height,
      original->spriteWidth,
      original->spriteHeight,
      &clone)) {
    return UFALSE;
  }
  memcpy(clone->pixels, original->pixels, original->width * original->height);
  *out = OBJ_VAL_EXPLICIT((Obj*)clone);
  return UTRUE;
}

static CFunction funcSpriteSheetClone = { implSpriteSheetClone, "clone" };

static void initSpriteSheetClass(ObjModule *module) {
  CFunction *methods[] = {
    &funcSpriteSheetClone,
    NULL,
  };
  newNativeClass(module, &descriptorSpriteSheet, methods, NULL);
}

#endif/*mtots_m_paco_ss_h*/
