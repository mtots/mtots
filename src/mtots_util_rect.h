#ifndef mtots_util_rect_h
#define mtots_util_rect_h

#include "mtots_common.h"

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

#endif/*mtots_util_rect_h*/
