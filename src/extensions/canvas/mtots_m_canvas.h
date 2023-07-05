#ifndef mtots_m_canvas_h
#define mtots_m_canvas_h

/* Native Module canvas */

#include "mtots_m_image.h"
#include "mtots_m_font.h"

#define AS_CANVAS(value) ((ObjCanvas*)AS_OBJ(value))
#define IS_CANVAS(value) (getNativeObjectDescriptor(value) == &descriptorCanvas)

typedef struct ObjCanvas {
  ObjNative obj;
  ObjImage *image;
  ObjPen *pen;
} ObjCanvas;

Value CANVAS_VAL(ObjCanvas *canvas);

ObjCanvas *newCanvas(ObjImage *image);
ubool canvasNewPen(ObjCanvas *canvas);
void addNativeModuleMediaCanvas();

extern NativeObjectDescriptor descriptorCanvas;

#endif/*mtots_m_canvas_h*/
