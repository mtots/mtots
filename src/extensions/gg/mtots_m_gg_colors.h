#ifndef mtots_m_ui_colors_h
#define mtots_m_ui_colors_h

#include "mtots_util_color.h"

/*
 * Some named colors for convenience.
 *
 * These are colors from PICO-8's palette.
 */

typedef struct ColorEntry {
  Color color;
  const char *name;
} ColorEntry;

static ColorEntry colorEntries[] = {
  /* PICO-8's official 16 colors */
  { {   0,   0,   0, 255 }, "BLACK" },
  { {  29,  43,  83, 255 }, "DARK_BLUE" },
  { { 126,  37,  83, 255 }, "DARK_PURPLE" },
  { {   0, 135,  81, 255 }, "DARK_GREEN" },
  { { 171,  82,  54, 255 }, "BROWN" },
  { {  95,  87,  79, 255 }, "DARK_GREY" },
  { { 194, 195, 199, 255 }, "LIGHT_GREY" },
  { { 255, 241, 232, 255 }, "WHITE" },
  { { 255,   0,  77, 255 }, "RED" },
  { { 255, 163,   0, 255 }, "ORANGE" },
  { { 255, 236,  39, 255 }, "YELLOW" },
  { {   0, 228,  54, 255 }, "GREEN" },
  { {  41, 173, 255, 255 }, "BLUE" },
  { { 131, 118, 156, 255 }, "LAVENDER" },
  { { 255, 119, 168, 255 }, "PINK" },
  { { 255, 204, 170, 255 }, "LIGHT_PEACH" },

  /* PICO-8's unofficial 16 colors */
  { {  41,  24,  20, 255 }, "BROWNISH_BLACK" },
  { {  17,  29,  53, 255 }, "DARKER_BLUE" },
  { {  66,  33,  54, 255 }, "DARKER_PURPLE" },
  { {  18,  83,  89, 255 }, "BLUE_GREEN" },
  { { 116,  47,  41, 255 }, "DARK_BROWN" },
  { {  73,  51,  59, 255 }, "DARKER_GREY" },
  { { 162, 136, 121, 255 }, "MEDIUM_GREY" },
  { { 243, 239, 125, 255 }, "LIGHT_YELLOW" },
  { { 190,  18,  80, 255 }, "DARK_RED" },
  { { 255, 108,  36, 255 }, "DARK_ORANGE" },
  { { 168, 231,  46, 255 }, "LIME_GREEN" },
  { {   0, 181,  67, 255 }, "MEDIUM_GREEN" },
  { {   6,  90, 181, 255 }, "TRUE_BLUE" },
  { { 117,  70, 101, 255 }, "MAUVE" },
  { { 255, 110,  89, 255 }, "DARK_PEACH" },
  { { 255, 157, 129, 255 }, "PEACH" },

  { {   0,   0,   0,   0 }, 0 },
};

#endif/*mtots_m_ui_colors_h*/
