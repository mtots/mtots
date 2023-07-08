#ifndef mtots_m_font_h
#define mtots_m_font_h

/* Native Module font */

#include "mtots_m_image.h"
#include "mtots_m_data.h"

#define AS_FONT(value) ((ObjFont*)AS_OBJ(value))
#define AS_PEN(value) ((ObjPen*)AS_OBJ(value))
#define IS_FONT(value) (getNativeObjectDescriptor(value) == &descriptorFont)
#define IS_PEN(value) (getNativeObjectDescriptor(value) == &descriptorPen)

typedef struct ObjFont ObjFont;
typedef struct ObjPen ObjPen;

Value FONT_VAL(ObjFont *font);
Value PEN_VAL(ObjPen *pen);
ubool newFontFromFile(const char *filePath, ObjFont **out);
ubool newFontFromMemory(Value underlying, const u8 *fontData, size_t fontDataLen, ObjFont **out);
ubool newFontFromData(ObjDataSource *ds, ObjFont **out);
size_t fontGetEmHeight(ObjFont *font);
ubool fontSetEmHeight(ObjFont *font, size_t newEmHeight);
ObjPen *newPen(ObjFont *font, ObjImage *image, double x, double y, Color color);

void addNativeModuleMediaFont(void);

extern NativeObjectDescriptor descriptorFont;
extern NativeObjectDescriptor descriptorPen;

#endif/*mtots_m_font_h*/
