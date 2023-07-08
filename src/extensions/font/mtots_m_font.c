#include "mtots_m_font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "mtots_vm.h"
#include "mtots_m_image.h"

static FT_Library ft;
static String *emWidthString;
static String *emHeightString;
static String *advanceHeightString;
static String *fontString;
static String *imageString;
static String *colorString;
static String *boundingBoxString;
static String *lineStartXString;

struct ObjFont {
  ObjNative obj;
  FT_Face face;

  /* pointer to an underlying object that holds some of the font data.
   * FT_Face instances opened with FT_New_Memory_Face do not
   * create copies of the buffer but instead hold references to it.
   * So ObjFont instances must keep their underlying alive */
  Value underlying;
};

struct ObjPen {
  ObjNative obj;
  ObjFont *font;
  ObjImage *image;
  float lineStartX, x, y;
  Color color;
  ObjRect *boundingBox;
};

static void blackenFont(ObjNative *n) {
  ObjFont *font = (ObjFont*)n;
  markValue(font->underlying);
}

static void freeFont(ObjNative *n) {
  ObjFont *font = (ObjFont*)n;
  FT_Done_Face(font->face);
}

static void blackenPen(ObjNative *n) {
  ObjPen *pen = (ObjPen*)n;
  markObject((Obj*)pen->font);
  markObject((Obj*)pen->image);
  markObject((Obj*)pen->boundingBox);
}

NativeObjectDescriptor descriptorFont = {
  blackenFont, freeFont, sizeof(ObjFont), "Font",
};

NativeObjectDescriptor descriptorPen = {
  blackenPen, nopFree, sizeof(ObjPen), "Pen",
};

static ubool fterr(const char *tag, int error) {
  if (error == FT_Err_Ok) {
    return UTRUE;
  }
  if (error) {
    switch (error) {

#define FT_NOERRORDEF_(name, code, message)
#define FT_ERRORDEF_(name, code, message) \
  case code: runtimeError("FreeType %s: %s", tag, message); break;

#include "freetype/fterrdef.h"

      default: runtimeError("FreeType %s: (unknown %d)", tag, error);

#undef FT_ERRORDEF_
#undef FT_NOERRORDEF_

    }
  }
  return UFALSE;
}

static ObjFont *allocFont(void) {
  ObjFont *font = NEW_NATIVE(ObjFont, &descriptorFont);
  font->underlying = NIL_VAL();
  return font;
}

Value FONT_VAL(ObjFont *font) {
  return OBJ_VAL_EXPLICIT((Obj*)font);
}

Value PEN_VAL(ObjPen *pen) {
  return OBJ_VAL_EXPLICIT((Obj*)pen);
}

ubool newFontFromFile(const char *filePath, ObjFont **out) {
  ObjFont *font = allocFont();
  FT_Error err = FT_New_Face(ft, filePath, 0, &font->face);
  if (!fterr("FT_New_Face", err)) {
    return UFALSE;
  }
  *out = font;
  return UTRUE;
}

ubool newFontFromMemory(Value underlying, const u8 *fontData, size_t fontDataLen, ObjFont **out) {
  ObjFont *font = allocFont();
  FT_Error err = FT_New_Memory_Face(ft, fontData, fontDataLen, 0, &font->face);
  if (!fterr("FT_New_Memory_Face", err)) {
    return UFALSE;
  }
  font->underlying = underlying;
  *out = font;
  return UTRUE;
}

ubool newFontFromData(ObjDataSource *ds, ObjFont **out) {
  switch (ds->type) {
    case DATA_SOURCE_STRING:
      /* This case is incredibl unlikely, since this would require that
       * the contents of a ttf be valid UTF-8 */
      return newFontFromMemory(
        STRING_VAL(ds->as.string),
        (const u8*)ds->as.string->chars,
        ds->as.string->byteLength,
        out);
    case DATA_SOURCE_FILE:
      return newFontFromFile(ds->as.file.path->chars, out);
    default: break;
  }
  {
    /* TODO: Use a Bytes object instead of ObjBuffer */
    ObjBuffer *buffer = newBuffer();
    ubool gcPause;
    LOCAL_GC_PAUSE(gcPause);
    if (!dataSourceReadIntoBuffer(ds, &buffer->handle)) {
      LOCAL_GC_UNPAUSE(gcPause);
      return UFALSE;
    }
    if (!newFontFromMemory(
        BUFFER_VAL(buffer),
        buffer->handle.data,
        buffer->handle.length,
        out)) {
      LOCAL_GC_UNPAUSE(gcPause);
      return UFALSE;
    }
    LOCAL_GC_UNPAUSE(gcPause);
  }
  return UTRUE;
}

size_t fontGetEmHeight(ObjFont *font) {
  return font->face->size->metrics.y_ppem;
}

ubool fontSetEmHeight(ObjFont *font, size_t newEmHeight) {
  FT_Error err = FT_Set_Pixel_Sizes(font->face, 0, newEmHeight);
  if (!fterr("FT_Set_Pixel_Sizes", err)) {
    return UFALSE;
  }
  return UTRUE;
}

ObjPen *newPen(ObjFont *font, ObjImage *image, double x, double y, Color color) {
  ObjPen *pen = NEW_NATIVE(ObjPen, &descriptorPen);
  ubool gcPause;
  LOCAL_GC_PAUSE(gcPause);
  pen->font = font;
  pen->image = image;
  pen->lineStartX = pen->x = x;
  pen->y = y;
  pen->color = color;
  pen->boundingBox = allocRect(newRect(0, 0, 0, 0));
  LOCAL_GC_UNPAUSE(gcPause);
  return pen;
}

static void blitGlyph(
    ObjImage *image,
    FT_Bitmap *bmp,
    i32 x,
    i32 y,
    Color textColor) {
  i32 nr = bmp->rows;
  i32 nc = bmp->width;
  i32 r, c;

  for (r = 0; r < nr; r++) {
    if (y + r < 0) {
      continue;
    }
    if (y + r >= image->height) {
      break;
    }
    for (c = 0; c < nc; c++) {
      Color *color;
      if (x + c < 0) {
        continue;
      }
      if (x + c >= image->width) {
        break;
      }
      {
        u32 alpha = bmp->buffer[r * bmp->pitch + c];
        color = image->pixels + ((y + r) * image->width + (x + c));
        color->red   = (color->red   * (255 - alpha) + textColor.red   * alpha) / 255;
        color->green = (color->green * (255 - alpha) + textColor.green * alpha) / 255;
        color->blue  = (color->blue  * (255 - alpha) + textColor.blue  * alpha) / 255;
        color->alpha = alpha > color->alpha ? alpha : color->alpha;
      }
    }
  }
}

static ubool penWrite(ObjPen *pen, String *string) {
  ObjFont *font = pen->font;
  ObjImage *image = pen->image;
  Color textColor = pen->color;
  size_t i;
  FT_GlyphSlot slot = font->face->glyph;
  i32 baseY = pen->y;
  float minX = pen->x;
  float maxX = minX;
  float minY = pen->y - font->face->size->metrics.y_ppem;
  float maxY = minY;
  for (i = 0; i < string->codePointCount; i++) {
    u32 ch = string->utf32 ? string->utf32[i] : (u32)string->chars[i];

    if (ch == '\n') {
      maxX = dmax(maxX, pen->x);
      baseY += font->face->size->metrics.height >> 6;
      pen->x = pen->lineStartX;
      pen->y = baseY;
      minX = dmin(minX, pen->x);
      continue;
    }

    if (!fterr(
        "FT_Load_Char", FT_Load_Char(font->face, ch, FT_LOAD_RENDER))) {
      return UFALSE;
    }

    if (image) {
      blitGlyph(
        image,
        &slot->bitmap,
        pen->x + slot->bitmap_left,
        pen->y - slot->bitmap_top,
        textColor);
    }

    pen->x += slot->advance.x >> 6;
    pen->y += slot->advance.y >> 6;
  }
  maxX = dmax(maxX, pen->x);
  maxY = dmax(maxY, pen->y - (font->face->size->metrics.descender >> 6));
  if (pen->boundingBox) {
    pen->boundingBox->handle.minX = minX;
    pen->boundingBox->handle.minY = minY;
    pen->boundingBox->handle.width = maxX - minX;
    pen->boundingBox->handle.height = maxY - minY;
  }
  return UTRUE;
}

static ubool implFontStaticFromData(i16 argc, Value *args, Value *out) {
  ObjDataSource *ds = AS_DATA_SOURCE(args[0]);
  ObjFont *font;
  if (!newFontFromData(ds, &font)) {
    return UFALSE;
  }
  *out = FONT_VAL(font);
  return UTRUE;
}

static TypePattern argsFontStaticFromData[] = {
  { TYPE_PATTERN_NATIVE, &descriptorDataSource },
};

static CFunction funcFontStaticFromData = {
  implFontStaticFromData, "fromData", 1, 0, argsFontStaticFromData
};

static ubool implFontStaticFromFile(i16 argc, Value *args, Value *out) {
  const char *filePath = AS_STRING(args[0])->chars;
  ObjFont *font;
  if (!newFontFromFile(filePath, &font)) {
    return UFALSE;
  }
  *out = FONT_VAL(font);
  return UTRUE;
}

static CFunction funcFontStaticFromFile = {
  implFontStaticFromFile, "fromFile", 1, 0, argsStrings
};

static ubool implFontStaticFromBuffer(i16 argc, Value *args, Value *out) {
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  ObjFont *font;
  bufferLock(&buffer->handle);
  if (!newFontFromMemory(
      BUFFER_VAL(buffer),
      buffer->handle.data,
      buffer->handle.length,
      &font)) {
    return UFALSE;
  }
  *out = FONT_VAL(font);
  return UTRUE;
}

static TypePattern argsFontStaticFromBuffer[] = {
  { TYPE_PATTERN_BUFFER },
};

static CFunction funcFontStaticFromBuffer = {
  implFontStaticFromBuffer, "fromBuffer", 1, 0, argsFontStaticFromBuffer
};

static ubool implFontGetattr(i16 argc, Value *args, Value *out) {
  ObjFont *font = AS_FONT(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == emHeightString) {
    *out = NUMBER_VAL(font->face->size->metrics.y_ppem);
  } else if (name == emWidthString) {
    *out = NUMBER_VAL(font->face->size->metrics.x_ppem);
  } else if (name == advanceHeightString) {
    *out = NUMBER_VAL(font->face->size->metrics.height >> 6);
  } else {
    runtimeError("Field %s not found on Font", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcFontGetattr = {
  implFontGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implFontSetattr(i16 argc, Value *args, Value *out) {
  ObjFont *font = AS_FONT(args[-1]);
  String *name = AS_STRING(args[0]);
  double value = AS_NUMBER(args[1]);
  if (name == emHeightString) {
    FT_Error err = FT_Set_Pixel_Sizes(font->face, 0, value);
    if (!fterr("FT_Set_Pixel_Sizes", err)) {
      return UFALSE;
    }
  } else if (name == emWidthString) {
    FT_Error err = FT_Set_Pixel_Sizes(font->face, value, 0);
    if (!fterr("FT_Set_Pixel_Sizes", err)) {
      return UFALSE;
    }
  } else {
    runtimeError("Field %s not found on Font", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static TypePattern argsFontSetattr[] = {
  { TYPE_PATTERN_STRING },
  { TYPE_PATTERN_NUMBER },
};

static CFunction funcFontSetattr = {
  implFontSetattr, "__setattr__", 2, 0, argsFontSetattr,
};

static ubool implFontNewPen(i16 argc, Value *args, Value *out) {
  ObjFont *font = AS_FONT(args[-1]);
  ObjImage *image = argc > 0 && !IS_NIL(args[0]) ? AS_IMAGE(args[0]) : NULL;
  double x = argc > 1 && !IS_NIL(args[1]) ? AS_NUMBER(args[1]) : 0;
  double y = argc > 2 && !IS_NIL(args[2]) ? AS_NUMBER(args[2]) : 0;
  Color color = argc > 3 && !IS_NIL(args[3]) ?
    AS_COLOR(args[3]) : newColor(255, 255, 255, 255);
  ObjPen *pen = newPen(font, image, x, y, color);
  *out = PEN_VAL(pen);
  return UTRUE;
}

static TypePattern argsFontNewPen[] = {
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorImage },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcFontNewPen = {
  implFontNewPen, "newPen", 0, 4, argsFontNewPen
};

static ubool implPenGetattr(i16 argc, Value *args, Value *out) {
  ObjPen *pen = AS_PEN(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == fontString) {
    *out = FONT_VAL(pen->font);
  } else if (name == imageString) {
    *out = IMAGE_VAL(pen->image);
  } else if (name == lineStartXString) {
    *out = NUMBER_VAL(pen->lineStartX);
  }  else if (name == vm.xString) {
    *out = NUMBER_VAL(pen->x);
  } else if (name == vm.yString) {
    *out = NUMBER_VAL(pen->y);
  } else if (name == colorString) {
    *out = COLOR_VAL(pen->color);
  } else if (name == boundingBoxString) {
    *out = RECT_VAL(pen->boundingBox);
  } else {
    runtimeError("Field %s not found on Pen", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcPenGetattr = {
  implPenGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implPenSetattr(i16 argc, Value *args, Value *out) {
  ObjPen *pen = AS_PEN(args[-1]);
  String *name = AS_STRING(args[0]);
  Value value = args[1];
  if (name == fontString) {
    if (!IS_FONT(value)) {
      runtimeError("Expected Font but got %s", getKindName(value));
      return UFALSE;
    }
    pen->font = AS_FONT(value);
  } else if (name == imageString) {
    if (!IS_NIL(value) && !IS_IMAGE(value)) {
      runtimeError("Expected Image but got %s", getKindName(value));
      return UFALSE;
    }
    pen->image = IS_NIL(value) ? NULL : AS_IMAGE(value);
  } else if (name == lineStartXString) {
    if (!IS_NUMBER(value)) {
      runtimeError("Expected Number but got %s", getKindName(value));
      return UFALSE;
    }
    pen->lineStartX = AS_NUMBER(value);
  } else if (name == vm.xString) {
    if (!IS_NUMBER(value)) {
      runtimeError("Expected Number but got %s", getKindName(value));
      return UFALSE;
    }
    pen->x = AS_NUMBER(value);
  } else if (name == vm.yString) {
    if (!IS_NUMBER(value)) {
      runtimeError("Expected Number but got %s", getKindName(value));
      return UFALSE;
    }
    pen->y = AS_NUMBER(value);
  } else if (name == colorString) {
    if (!IS_COLOR(value)) {
      runtimeError("Expected Color but got %s", getKindName(value));
      return UFALSE;
    }
    pen->color = AS_COLOR(value);
  } else if (name == boundingBoxString) {
    if (!IS_RECT(value)) {
      runtimeError("Expected Rect but got %s", getKindName(value));
      return UFALSE;
    }
    pen->boundingBox = AS_RECT(value);
  } else {
    runtimeError("Field %s not found on Pen", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcPenSetattr = {
  implPenSetattr, "__setattr__", 2, 0, argsSetattr
};

static ubool implPenWrite(i16 argc, Value *args, Value *out) {
  ObjPen *pen = AS_PEN(args[-1]);
  String *text = AS_STRING(args[0]);
  return penWrite(pen, text);
}

static CFunction funcPenWrite = {
  implPenWrite, "write", 1, 0, argsStrings
};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *staticFontMethods[] = {
    &funcFontStaticFromData,
    &funcFontStaticFromFile,
    &funcFontStaticFromBuffer,
    NULL,
  };
  CFunction *fontMethods[] = {
    &funcFontGetattr,
    &funcFontSetattr,
    &funcFontNewPen,
    NULL,
  };
  CFunction *staticPenMethods[] = {
    NULL,
  };
  CFunction *penMethods[] = {
    &funcPenGetattr,
    &funcPenSetattr,
    &funcPenWrite,
    NULL,
  };

  moduleRetain(module, STRING_VAL(emWidthString = internCString("emWidth")));
  moduleRetain(module, STRING_VAL(emHeightString = internCString("emHeight")));
  moduleRetain(module, STRING_VAL(advanceHeightString = internCString("advanceHeight")));
  moduleRetain(module, STRING_VAL(fontString = internCString("font")));
  moduleRetain(module, STRING_VAL(imageString = internCString("image")));
  moduleRetain(module, STRING_VAL(colorString = internCString("color")));
  moduleRetain(module, STRING_VAL(boundingBoxString = internCString("boundingBox")));
  moduleRetain(module, STRING_VAL(lineStartXString = internCString("lineStartX")));

  if (!importModuleAndPop("media.image")) {
    return UFALSE;
  }

  if (!fterr("FT_Init_FreeType", FT_Init_FreeType(&ft))) {
    return UFALSE;
  }

  newNativeClass(module, &descriptorFont, fontMethods, staticFontMethods);
  newNativeClass(module, &descriptorPen, penMethods, staticPenMethods);

  return UTRUE;
}

static CFunction func = { impl, "media.font", 1 };

void addNativeModuleMediaFont(void) {
  addNativeModule(&func);
}
