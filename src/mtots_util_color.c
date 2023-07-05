#include "mtots_util_color.h"


Color newColor(u8 red, u8 green, u8 blue, u8 alpha) {
  Color color;
  color.red = red;
  color.green = green;
  color.blue = blue;
  color.alpha = alpha;
  return color;
}

ubool colorEquals(Color a, Color b) {
  return a.red == b.red &&
    a.green == b.green &&
    a.blue == b.blue &&
    a.alpha == b.alpha;
}
