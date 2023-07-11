#ifndef mtots_util_rect_h
#define mtots_util_rect_h

#include "mtots_common.h"

typedef struct RectExponent {
  i8 x, y, w, h;
} RectExponent;

typedef struct RectFraction {
  i16 x, y, w, h;
} RectFraction;

/*
 * Rectangle
 *
 * More specifically, 2D AABB (axis aligned bounding box).
 */
typedef struct Rect {
  float minX;
  float minY;
  float width;
  float height;
} Rect;

Rect newRect(float minX, float minY, float width, float height);
Rect newRectFromParts(RectExponent exp, RectFraction fr);
void rectToParts(Rect rect, RectExponent *exp, RectFraction *fr);
ubool rectEquals(Rect a, Rect b);

#endif/*mtots_util_rect_h*/
