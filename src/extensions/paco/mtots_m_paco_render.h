#ifndef mtots_m_paco_render_h
#define mtots_m_paco_render_h

#include "mtots_m_paco_common.h"
#include "mtots_m_paco_ss.h"

#define DRAW_STYLE_FILL                                    0
#define DRAW_STYLE_OUTLINE                                 1
#define SPRITE_OUT_OF_BOUNDS_COLOR                        17
#define COLOR_COUNT                                       32 /* must be power of 2 */
#define IS_TRANSPARENT(color)                  ((color)==TRANSPARENT)
#define APPLY_PALETTE(rt,color)                ((rt)->palette[color])

typedef struct RenderTarget {
  size_t width;
  size_t height;
  u8 *framebuffer;
  size_t clipXStart;
  size_t clipXEnd;
  size_t clipYStart;
  size_t clipYEnd;
  u8 *palette;
} RenderTarget;

static i32 iabs(i32 x) {
  return x < 0 ? -x : x;
}

static void setPixel(RenderTarget *rt, i32 x, i32 y, u8 color) {
  if (x < rt->clipXStart || y < rt->clipYStart || x >= rt->clipXEnd || y >= rt->clipYEnd) {
    return;
  }
  color = APPLY_PALETTE(rt, color);
  if (!IS_TRANSPARENT(color)) {
    rt->framebuffer[y * rt->width + x] = color;
  }
}

static u8 getPixel(RenderTarget *rt, i32 x, i32 y) {
  if (x < rt->clipXStart || y < rt->clipYStart || x >= rt->clipXEnd || y >= rt->clipYEnd) {
    return 0;
  }
  return rt->framebuffer[y * rt->width + x];
}

static ubool drawRect(RenderTarget *rt, i32 x0, i32 y0, i32 x1, i32 y1, u8 style, u8 color) {
  i32 firstX, lastX, width, firstY, lastY, y;
  if (x1 < x0 || y1 < y0 ||
      x0 >= rt->clipXEnd || x1 < rt->clipXStart ||
      y0 >= rt->clipYEnd || y1 < rt->clipYStart) {
    return UTRUE;
  }
  color = APPLY_PALETTE(rt, color);
  if (IS_TRANSPARENT(color)) {
    return UTRUE;
  }
  firstX = x0 >= rt->clipXStart ? x0 : rt->clipXStart;
  lastX = x1 < rt->clipXEnd ? x1 : rt->clipXEnd - 1;
  width = lastX - firstX + 1;
  firstY = y0 >= rt->clipYStart ? y0 : rt->clipYStart;
  lastY = y1 < rt->clipYEnd ? y1 : rt->clipYEnd - 1;
  switch (style) {
    case DRAW_STYLE_FILL: {
      for (y = firstY; y <= lastY; y++) {
        memset(rt->framebuffer + (y * rt->width + firstX), color, width);
      }
      break;
    }
    case DRAW_STYLE_OUTLINE: {
      if (y0 >= rt->clipYStart) {
        memset(rt->framebuffer + (y0 * rt->width + firstX), color, width);
      }
      if (y1 < rt->clipYEnd) {
        memset(rt->framebuffer + (y1 * rt->width + firstX), color, width);
      }
      if (x0 >= rt->clipXStart) {
        for (y = firstY; y <= lastY; y++) {
          rt->framebuffer[y * rt->width + x0] = color;
        }
      }
      if (x1 < rt->clipXEnd) {
        for (y = firstY; y <= lastY; y++) {
          rt->framebuffer[y * rt->width + x1] = color;
        }
      }
      break;
    }
    default:
      runtimeError("Unrecognized draw style %d", style);
  }
  return UTRUE;
}

static ubool drawLine(RenderTarget *rt, i32 x0, i32 y0, i32 x1, i32 y1, u8 style, u8 color) {
  i32 stepX = x0 < x1 ? 1 : -1;
  i32 stepY = y0 < y1 ? 1 : -1;
  if (style != DRAW_STYLE_OUTLINE) {
    runtimeError("drawLine currently only supports the OUTLINE (1) draw style");
    return UFALSE;
  }
  if (iabs(y1 - y0) == iabs(x1 - x0)) {
    /* abs(slope) = 1 or undefined
     *
     * This covers the cases:
     *   - (x0, y0) == (x1, y1), and
     *   - the line is exactly along a diagonal
     */
    i32 x, y;
    for (x = x0, y = y0; x != x1 + stepX; x += stepX, y += stepY) {
      setPixel(rt, x, y, color);
    }
  } else if (iabs(y1 - y0) < iabs(x1 - x0)) {
    /* abs(slope) < 1 (and abs(x1 - x0) > 0)
     *
     * In this case, we fill exactly 1 pixel for each x value between
     * x0 and x1. The main problem is to find the y coordinate for each
     * x coordinate.
     *
     * We do this by comparing the two candidates for y at each x:
     *   * we can use the same y as the previous x, or
     *   * we can add stepY to the previous y
     *
     * We pick the choice that produces a smaller error when compared
     * with the true value of y at the given x.
     */
    i32 x = x0 + stepX, y = y0;
    setPixel(rt, x0, y0, color);
    for (; x != x1 + stepX; x += stepX) {
      /*  y                   = (y1 - y0) / (x1 - x0) * (x - x0) +  y0
       *  y - y'              = (y1 - y0) / (x1 - x0) * (x - x0) +  y0 - y'
       * (y - y') * (x1 - x0) = (y1 - y0)             * (x - x0) + (y0 - y') * (x1 - x0)
       */
      i32 e0 = iabs((y1 - y0) * (x - x0) + (y0 -  y         ) * (x1 - x0));
      i32 e1 = iabs((y1 - y0) * (x - x0) + (y0 - (y + stepY)) * (x1 - x0));
      if (e1 < e0) {
        y += stepY;
      }
      setPixel(rt, x, y, color);
    }
  } else {
    /* abs(slope) > 1 (and abs(y1 - y0) > 0)
     *
     * Same as above, but the axes swapped
     *
     *  x                   = (x1 - x0) / (y1 - y0) * (y - y0) +  x0
     *  x - x'              = (x1 - x0) / (y1 - y0) * (y - y0) +  x0 - x'
     * (x - x') * (y1 - y0) = (x1 - x0)             * (y - y0) + (x0 - x') * (y1 - y0)
     */
    i32 x = x0, y = y0 + stepY;
    setPixel(rt, x0, y0, color);
    for (; y != y1 + stepY; y += stepY) {
      i32 e0 = iabs((x1 - x0) * (y - y0) + (x0 -  x         ) * (y1 - y0));
      i32 e1 = iabs((x1 - x0) * (y - y0) + (x0 - (x + stepX)) * (y1 - y0));
      if (e1 < e0) {
        x += stepX;
      }
      setPixel(rt, x, y, color);
    }
  }
  return UTRUE;
}

static ubool drawEllipse(RenderTarget *rt, i32 cx, i32 cy, i32 a, i32 b, u8 style, u8 color) {
  /*
   * Modified Bresenham's Circle Algorithm
   * https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
   *
   * In the future, if we want to allow larger values of `a` and `b`,
   * we might want to ditch Bresenham, and just use floating point
   * arithmetic.
   */
  i32 dx = iabs(a);
  i32 dy = 0;
  i32 a2 = a * a;
  i32 b2 = b * b;
  i32 a2b2 = a2 * b2;
  ubool fill = style == DRAW_STYLE_FILL;
  b = iabs(b);

  if (iabs(a) > I8_MAX || iabs(b) > I8_MAX) {
    runtimeError(
      "drawEllipse() requires that the semiaxis be <= %d, but got a=%d, b=%d",
      (int)I8_MAX, (int)a, (int)b);
    return UFALSE;
  }

  while (dx >= 0 && dy <= b) {
    /*
     * While with a circle, we have eight symmetric slices, with an ellipse
     * we are only guaranteed four.
     */
    if (fill) {
      i32 px;
      for (px = cx - dx; px <= cx + dx; px++) {
        setPixel(rt, px, cy - dy, color);
        setPixel(rt, px, cy + dy, color);
      }
    } else {
      setPixel(rt, cx - dx, cy - dy, color);
      setPixel(rt, cx + dx, cy - dy, color);
      setPixel(rt, cx - dx, cy + dy, color);
      setPixel(rt, cx + dx, cy + dy, color);
    }

    {
      i32 re1 = iabs(b2 *       dx *       dx + a2 * (dy + 1) * (dy + 1) - a2b2);
      i32 re2 = iabs(b2 * (dx - 1) * (dx - 1) + a2 *       dy *       dy - a2b2);
      i32 re3 = iabs(b2 * (dx - 1) * (dx - 1) + a2 * (dy + 1) * (dy + 1) - a2b2);
      if (re3 <= re1 && re3 <= re2) {
        dx--;
        dy++;
      } else if (re1 < re2) {
        dy++;
      } else {
        dx--;
      }
    }
  }

  return UTRUE;
}

static ubool drawSheet(
    RenderTarget *rt,
    ObjSpriteSheet *ss,
    i32 sx0, i32 sy0, i32 sw, i32 sh,
    i32 dx0, i32 dy0, i32 dw, i32 dh,
    ubool flipX, ubool flipY) {
  i32 sx1 = sx0 + sw;
  i32 sy1 = sy0 + sh;
  i32 dx1 = dx0 + dw;
  i32 dy1 = dy0 + dh;
  i32 i, j, iStart = 0, iEnd = dw, jStart = 0, jEnd = dh;
  if (sw < 0 || sh < 0 || dw < 0 || dh < 0 ||
      sx0 >= ss->width || sx1 < 0 || sy0 >= ss->height || sy1 < 0 ||
      dx0 >= rt->clipXEnd  || dx1 < rt->clipXStart ||
      dy0 >= rt->clipYEnd  || dy1 < rt->clipYStart) {
    return UTRUE;
  }
  if (dy0 + jStart < rt->clipYStart) {
    jStart = rt->clipYStart - dy0;
  }
  if (dy0 + jEnd > rt->clipYEnd) {
    jEnd = rt->clipYEnd - dy0;
  }
  if (dx0 + iStart < rt->clipXStart) {
    iStart = rt->clipXStart - dx0;
  }
  if (dx0 + iEnd > rt->clipXEnd) {
    iEnd = rt->clipXEnd - dx0;
  }
  for (j = jStart; j < jEnd; j++) {
    i32 dy = dy0 + j;
    i32 sy = sy0 + (flipY ? (dh - 1 - j) : j) * sh / dh;
    for (i = iStart; i < iEnd; i++) {
      i32 dx = dx0 + i;
      i32 sx = sx0 + (flipX ? (dw - 1 - i) : i) * sw / dw;
      u8 color;
      color = sy < 0 || sy >= ss->height || sx < 0 || sx >= ss->width ?
        SPRITE_OUT_OF_BOUNDS_COLOR : ss->pixels[sy * ss->width + sx];
      color = APPLY_PALETTE(rt, color);
      if (!IS_TRANSPARENT(color)) {
        rt->framebuffer[dy * rt->width + dx] = color;
      }
    }
  }
  return UTRUE;
}

static ubool drawSprite(
    RenderTarget *rt, ObjSpriteSheet *ss,
    i32 n, i32 x, i32 y, i32 w, i32 h, ubool flipX, ubool flipY) {
  if (n < 0 || n >= (ss->height / ss->spriteHeight) * (ss->width / ss->spriteWidth)) {
    runtimeError("invalid sprite index");
    return UFALSE;
  }
  return drawSheet(
    rt, ss,
    (n % (ss->width / ss->spriteWidth)) * ss->spriteWidth,
    (n / (ss->width / ss->spriteWidth)) * ss->spriteHeight,
    ss->spriteWidth * w,
    ss->spriteHeight * h,
    x, y,
    ss->spriteWidth * w,
    ss->spriteHeight * h,
    flipX, flipY);
}

#endif/*mtots_m_paco_render_h*/
