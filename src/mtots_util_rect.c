#include "mtots_util_rect.h"

Rect newRect(float minX, float minY, float width, float height) {
  Rect ret;
  ret.minX = minX;
  ret.minY = minY;
  ret.width = width;
  ret.height = height;
  return ret;
}
