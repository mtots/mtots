#include "mtots_util_rect.h"

#include "mtots_util_number.h"

Rect newRect(float minX, float minY, float width, float height) {
  Rect ret;
  ret.minX = minX;
  ret.minY = minY;
  ret.width = width;
  ret.height = height;
  return ret;
}

Rect newRectFromParts(RectExponent exp, RectFraction fr) {
  Rect ret;
  ret.minX = partsToF24(exp.x, fr.x);
  ret.minY = partsToF24(exp.y, fr.y);
  ret.width = partsToF24(exp.w, fr.w);
  ret.height = partsToF24(exp.h, fr.h);
  return ret;
}

void rectToParts(Rect rect, RectExponent *exp, RectFraction *fr) {
  f24ToParts(rect.minX, &exp->x, &fr->x);
  f24ToParts(rect.minY, &exp->y, &fr->y);
  f24ToParts(rect.width, &exp->w, &fr->w);
  f24ToParts(rect.height, &exp->h, &fr->h);
}

ubool rectEquals(Rect a, Rect b) {
  return a.minX == b.minX &&
    a.minY == b.minY &&
    a.width == b.width &&
    a.height == b.height;
}
