#ifndef mtots_util_color_h
#define mtots_util_color_h

#include "mtots_common.h"

typedef struct Color {
  u8 red;
  u8 green;
  u8 blue;
  u8 alpha;
} Color;

Color newColor(u8 red, u8 green, u8 blue, u8 alpha);
ubool colorEquals(Color a, Color b);

#endif/*mtots_util_color_h*/
