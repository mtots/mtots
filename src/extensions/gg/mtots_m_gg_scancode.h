#ifndef mtots_m_ui_scancode_h
#define mtots_m_ui_scancode_h

/*
 * USB HID Keyboard scan codes as per USB spec
 *
 * From:
 * https://gist.github.com/ekaitz-zarraga/2b25b94b711684ba4e969e5a5723969b
 *
 * This is also what SDL2 seems to respect when it comes to keyboard
 * scancodes.
 *
 * These really should match SDL's scancodes
 */

typedef struct ScancodeEntry {
  const char *name;
  int scancode;
} ScancodeEntry;

static ScancodeEntry scancodeEntries[] = {
  { "A", 0x04 }, /* Keyboard a and A */
  { "B", 0x05 }, /* Keyboard b and B */
  { "C", 0x06 }, /* Keyboard c and C */
  { "D", 0x07 }, /* Keyboard d and D */
  { "E", 0x08 }, /* Keyboard e and E */
  { "F", 0x09 }, /* Keyboard f and F */
  { "G", 0x0A }, /* Keyboard g and G */
  { "H", 0x0B }, /* Keyboard h and H */
  { "I", 0x0C }, /* Keyboard i and I */
  { "J", 0x0D }, /* Keyboard j and J */
  { "K", 0x0E }, /* Keyboard k and K */
  { "L", 0x0F }, /* Keyboard l and L */
  { "M", 0x10 }, /* Keyboard m and M */
  { "N", 0x11 }, /* Keyboard n and N */
  { "O", 0x12 }, /* Keyboard o and O */
  { "P", 0x13 }, /* Keyboard p and P */
  { "Q", 0x14 }, /* Keyboard q and Q */
  { "R", 0x15 }, /* Keyboard r and R */
  { "S", 0x16 }, /* Keyboard s and S */
  { "T", 0x17 }, /* Keyboard t and T */
  { "U", 0x18 }, /* Keyboard u and U */
  { "V", 0x19 }, /* Keyboard v and V */
  { "W", 0x1A }, /* Keyboard w and W */
  { "X", 0x1B }, /* Keyboard x and X */
  { "Y", 0x1C }, /* Keyboard y and Y */
  { "Z", 0x1D }, /* Keyboard z and Z */

  { "1", 0x1E }, /* Keyboard 1 and ! */
  { "2", 0x1F }, /* Keyboard 2 and @ */
  { "3", 0x20 }, /* Keyboard 3 and # */
  { "4", 0x21 }, /* Keyboard 4 and $ */
  { "5", 0x22 }, /* Keyboard 5 and % */
  { "6", 0x23 }, /* Keyboard 6 and ^ */
  { "7", 0x24 }, /* Keyboard 7 and & */
  { "8", 0x25 }, /* Keyboard 8 and * */
  { "9", 0x26 }, /* Keyboard 9 and ( */
  { "0", 0x27 }, /* Keyboard 0 and ) */

  { "ENTER",       0x28 }, /* Keyboard Return (ENTER) */
  { "ESC",         0x29 }, /* Keyboard ESCAPE */
  { "BACKSPACE",   0x2A }, /* Keyboard DELETE (Backspace) */
  { "TAB",         0x2B }, /* Keyboard Tab */
  { "SPACE",       0x2C }, /* Keyboard Spacebar */
  { "MINUS",       0x2D }, /* Keyboard - and _ */
  { "EQUAL",       0x2E }, /* Keyboard = and + */
  { "LEFTBRACE",   0x2F }, /* Keyboard [ and { */
  { "RIGHTBRACE",  0x30 }, /* Keyboard ] and } */
  { "BACKSLASH",   0x31 }, /* Keyboard \ and | */
  { "HASHTILDE",   0x32 }, /* Keyboard Non-US # and ~ */
  { "SEMICOLON",   0x33 }, /* Keyboard ; and : */
  { "APOSTROPHE",  0x34 }, /* Keyboard ' and " */
  { "GRAVE",       0x35 }, /* Keyboard ` and ~ */
  { "COMMA",       0x36 }, /* Keyboard , and < */
  { "DOT",         0x37 }, /* Keyboard . and > */
  { "SLASH",       0x38 }, /* Keyboard / and ? */
  { "CAPSLOCK",    0x39 }, /* Keyboard Caps Lock */

  { "F1",          0x3a }, /* Keyboard F1 */
  { "F2",          0x3b }, /* Keyboard F2 */
  { "F3",          0x3c }, /* Keyboard F3 */
  { "F4",          0x3d }, /* Keyboard F4 */
  { "F5",          0x3e }, /* Keyboard F5 */
  { "F6",          0x3f }, /* Keyboard F6 */
  { "F7",          0x40 }, /* Keyboard F7 */
  { "F8",          0x41 }, /* Keyboard F8 */
  { "F9",          0x42 }, /* Keyboard F9 */
  { "F10",         0x43 }, /* Keyboard F10 */
  { "F11",         0x44 }, /* Keyboard F11 */
  { "F12",         0x45 }, /* Keyboard F12 */

  { "SYSRQ",       0x46 }, /* Keyboard Print Screen */
  { "SCROLLLOCK",  0x47 }, /* Keyboard Scroll Lock */
  { "PAUSE",       0x48 }, /* Keyboard Pause */
  { "INSERT",      0x49 }, /* Keyboard Insert */
  { "HOME",        0x4A }, /* Keyboard Home */
  { "PAGEUP",      0x4B }, /* Keyboard Page Up */
  { "DELETE",      0x4C }, /* Keyboard Delete Forward */
  { "END",         0x4D }, /* Keyboard End */
  { "PAGEDOWN",    0x4E }, /* Keyboard Page Down */
  { "RIGHT",       0x4F }, /* Keyboard Right Arrow */
  { "LEFT",        0x50 }, /* Keyboard Left Arrow */
  { "DOWN",        0x51 }, /* Keyboard Down Arrow */
  { "UP",          0x52 }, /* Keyboard Up Arrow */

  { "NUMLOCK",     0x53 }, /* Keyboard Num Lock and Clear */
  { "KPSLASH",     0x54 }, /* Keypad / */
  { "KPASTERISK",  0x55 }, /* Keypad * */
  { "KPMINUS",     0x56 }, /* Keypad - */
  { "KPPLUS",      0x57 }, /* Keypad + */
  { "KPENTER",     0x58 }, /* Keypad ENTER */
  { "KP1",         0x59 }, /* Keypad 1 and End */
  { "KP2",         0x5a }, /* Keypad 2 and Down Arrow */
  { "KP3",         0x5b }, /* Keypad 3 and PageDn */
  { "KP4",         0x5c }, /* Keypad 4 and Left Arrow */
  { "KP5",         0x5d }, /* Keypad 5 */
  { "KP6",         0x5e }, /* Keypad 6 and Right Arrow */
  { "KP7",         0x5f }, /* Keypad 7 and Home */
  { "KP8",         0x60 }, /* Keypad 8 and Up Arrow */
  { "KP9",         0x61 }, /* Keypad 9 and Page Up */
  { "KP0",         0x62 }, /* Keypad 0 and Insert */
  { "KPDOT",       0x63 }, /* Keypad . and Delete */

  { "LEFTCTRL",    0xe0 }, /* Keyboard Left Control */
  { "LEFTSHIFT",   0xe1 }, /* Keyboard Left Shift */
  { "LEFTALT",     0xe2 }, /* Keyboard Left Alt */
  { "LEFTMETA",    0xe3 }, /* Keyboard Left GUI */
  { "RIGHTCTRL",   0xe4 }, /* Keyboard Right Control */
  { "RIGHTSHIFT",  0xe5 }, /* Keyboard Right Shift */
  { "RIGHTALT",    0xe6 }, /* Keyboard Right Alt */
  { "RIGHTMETA",   0xe7 }, /* Keyboard Right GUI */

  { 0, 0 },
};

#endif/*mtots_m_ui_scancode_h*/
