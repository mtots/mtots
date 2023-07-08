#include "mtots_m_canvas.h"

#include "mtots_vm.h"

#include "mtots_m_fontrm.h"
#include "mtots_m_png.h"

#include <stdlib.h>

#define DRAW_STYLE_FILL                                    0
#define DRAW_STYLE_OUTLINE                                 1

#define DEFAULT_PEN_FONT_SIZE 32

#define WIDTHOF(canvas) ((canvas)->image->width)
#define HEIGHTOF(canvas) ((canvas)->image->height)
#define PIXELSOF(canvas) ((canvas)->image->pixels)

static String *imageString;

static void blackenCanvas(ObjNative *n) {
  ObjCanvas *canvas = (ObjCanvas*)n;
  markObject((Obj*)canvas->image);
}

NativeObjectDescriptor descriptorCanvas = {
  blackenCanvas, nopFree, sizeof(ObjCanvas), "Canvas",
};

Value CANVAS_VAL(ObjCanvas *canvas) {
  return OBJ_VAL_EXPLICIT((Obj*)canvas);
}

/*
 * Creates a new canvas with given width and height
 * NOTE: the pixel data is not set to any particular value.
 * You should overwrite them or zero them out after this call.
 */
ObjCanvas *newCanvas(ObjImage *image) {
  ObjCanvas *canvas = NEW_NATIVE(ObjCanvas, &descriptorCanvas);
  canvas->image = image;
  canvas->pen = NULL;
  return canvas;
}

ubool canvasNewPen(ObjCanvas *canvas) {
  ObjFont *font;
  ubool gcPause;

  if (!loadRobotoMonoFont(&font)) {
    return UFALSE;
  }
  if (!fontSetEmHeight(font, DEFAULT_PEN_FONT_SIZE)) {
    return UFALSE;
  }
  LOCAL_GC_PAUSE(gcPause);
  canvas->pen =
    newPen(font, canvas->image, 0, fontGetEmHeight(font), newColor(255, 255, 255, 255));
  LOCAL_GC_UNPAUSE(gcPause);
  return UTRUE;
}

static void setPixel(ObjCanvas *canvas, i32 x, i32 y, Color color) {
  ObjImage *image = canvas->image;
  if (x < 0 || x >= image->width || y < 0 || y >= image->height) {
    return;
  }
  image->pixels[y * image->width + x] = color;
}

static i32 iabs(i32 x) {
  return x < 0 ? -x : x;
}

static ubool drawLine(
    ObjCanvas *canvas,
    i32 x0,
    i32 y0,
    i32 x1,
    i32 y1,
    Color color) {
  i32 stepX = x0 < x1 ? 1 : -1;
  i32 stepY = y0 < y1 ? 1 : -1;
  if (iabs(y1 - y0) == iabs(x1 - x0)) {
    /* abs(slope) = 1 or undefined
     *
     * This covers the cases:
     *   - (x0, y0) == (x1, y1), and
     *   - the line is exactly along a diagonal
     */
    i32 x, y;
    for ((void)(x = x0), y = y0; x != x1 + stepX; x += stepX, y += stepY) {
      setPixel(canvas, x, y, color);
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
    setPixel(canvas, x0, y0, color);
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
      setPixel(canvas, x, y, color);
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
    setPixel(canvas, x0, y0, color);
    for (; y != y1 + stepY; y += stepY) {
      i32 e0 = iabs((x1 - x0) * (y - y0) + (x0 -  x         ) * (y1 - y0));
      i32 e1 = iabs((x1 - x0) * (y - y0) + (x0 - (x + stepX)) * (y1 - y0));
      if (e1 < e0) {
        x += stepX;
      }
      setPixel(canvas, x, y, color);
    }
  }
  return UTRUE;
}

static ubool drawRect(ObjCanvas *canvas, Rect *rect, u8 style, Color color) {
  i32 minX = dmax(0, rect->minX);
  i32 maxX = dmin(WIDTHOF(canvas) - 1, rect->minX + rect->width - 1);
  i32 minY = dmax(0, rect->minY);
  i32 maxY = dmin(HEIGHTOF(canvas) - 1, rect->minY + rect->height - 1);
  i32 stride = WIDTHOF(canvas);
  i32 x, y;
  switch (style) {
    case DRAW_STYLE_FILL: {
      for (y = minY; y <= maxY; y++) {
        for (x = minX; x <= maxX; x++) {
          PIXELSOF(canvas)[y * stride + x] = color;
        }
      }
      break;
    }
    case DRAW_STYLE_OUTLINE: {
      if (rect->minX == minX) {
        for (y = minY; y <= maxY; y++) {
          PIXELSOF(canvas)[y * stride + minX] = color;
        }
      }
      if (rect->minX + rect->width - 1 == maxX) {
        for (y = minY; y <= maxY; y++) {
          PIXELSOF(canvas)[y * stride + maxX] = color;
        }
      }
      if (rect->minY == minY) {
        for (x = minX; x <= maxX; x++) {
          PIXELSOF(canvas)[minY * stride + x] = color;
        }
      }
      if (rect->minY + rect->height - 1 == maxY) {
        for (x = minX; x <= maxX; x++) {
          PIXELSOF(canvas)[maxY * stride + x] = color;
        }
      }
      break;
    }
    default:
      runtimeError("Unrecognized draw style %d", style);
  }
  return UTRUE;
}

static ubool drawEllipse(
    ObjCanvas *canvas,
    double cx,
    double cy,
    double a,
    double b,
    u8 style,
    Color color) {
  /*
   * Modified Bresenham's Circle Algorithm
   * https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
   *
   * In the future, if we want to allow larger values of `a` and `b`,
   * we might want to ditch Bresenham, and just use floating point
   * arithmetic.
   */
  double dx = dabs(a);
  double dy = 0;
  double a2 = a * a;
  double b2 = b * b;
  double a2b2 = a2 * b2;
  ubool fill = style == DRAW_STYLE_FILL;
  b = dabs(b);

  while (dx >= 0 && dy <= b) {
    /*
     * While with a circle, we have eight symmetric slices, with an ellipse
     * we are only guaranteed four.
     */
    if (fill) {
      double px;
      for (px = cx - dx; px <= cx + dx; px++) {
        setPixel(canvas, px, cy - dy, color);
        setPixel(canvas, px, cy + dy, color);
      }
    } else {
      setPixel(canvas, cx - dx, cy - dy, color);
      setPixel(canvas, cx + dx, cy - dy, color);
      setPixel(canvas, cx - dx, cy + dy, color);
      setPixel(canvas, cx + dx, cy + dy, color);
    }

    {
      double re1 = dabs(b2 *       dx *       dx + a2 * (dy + 1) * (dy + 1) - a2b2);
      double re2 = dabs(b2 * (dx - 1) * (dx - 1) + a2 *       dy *       dy - a2b2);
      double re3 = dabs(b2 * (dx - 1) * (dx - 1) + a2 * (dy + 1) * (dy + 1) - a2b2);
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

static int compareFloat(const void *a, const void *b) {
  float x = *(const float*)a;
  float y = *(const float*)b;
  return x == y ? 0 : x < y ? -1 : 1;
}

static i32 firound(float x) {
  return (i32)(x + 0.5);
}

static ubool fillPolygon(ObjCanvas *canvas, size_t n, float *c, Color color) {
  float *buf;
  float minX, minY, maxX, maxY;
  i32 x, y, iminY, imaxY;
  size_t i, ln;

  if (n < 3) {
    runtimeError("A polygon requires at least 3 vertices but got %d", (int)n);
    return UFALSE;
  }

  buf = (float*)malloc(sizeof(float) * n);

  minX = maxX = c[0];
  minY = maxY = c[1];
  for (i = 0; i < n; i++) {
    minX = dmin(minX, c[2 * i + 0]);
    maxX = dmax(maxX, c[2 * i + 0]);
    minY = dmin(minY, c[2 * i + 1]);
    maxY = dmax(maxY, c[2 * i + 1]);
  }
  iminY = firound(minY);
  imaxY = firound(maxY);

  for (y = iminY; y <= imaxY; y++) {
    ln = 0;
    for (i = 0; i < n; i++) {
      float y0 = c[2 * (i == 0 ? n - 1 : i - 1) + 1];
      float x1 = c[2 * i + 0];
      float y1 = c[2 * i + 1];
      float x2 = c[2 * (i == n - 1 ? 0 : i + 1) + 0];
      float y2 = c[2 * (i == n - 1 ? 0 : i + 1) + 1];

      /* If both y coordinates are on the same side of the scanline,
       * there's no way it crosses the scanline */
      if ((y1 > y && y2 > y) || (y1 < y && y2 < y)) {
        continue;
      }

      /* The line is parallel to the scanline.
       *
       * Given the previous check, this means that the
       * line overlaps with the scanline
       *
       * We still run the rest of this iteration because we still
       * want to process the logic for determining scanline crossings */
      if (y1 == y2) {
        i32 startX = firound(x1 < x2 ? x1 : x2);
        i32 endX = firound(x1 > x2 ? x1 : x2);
        for (x = startX; x <= endX; x++) {
          setPixel(canvas, x, y, color);
        }
      }

      /* If the current scanline crosses the current point,
       * check to see if this point counts as a "crossing" by
       * looking at the previous line */
      if (y1 == y) {
        if (y0 == y || y2 == y || (y0 < y && y2 > y) || (y0 > y && y2 < y)) {
          buf[ln++] = x1;
        }
        continue;
      }

      /* If p2 crosses the endpoint, deal with this in the next iteration */
      if (y2 == y) {
        continue;
      }

      /*
       * point slope form
       *
       * (y - y1) = m(x - x1)
       * (y - y1)/m + x1 = x
       * m = (y2 - y1)/(x2 - x1), so
       * x = (y - y1)*(x2 - x1)/(y2 - y1) + x1
       */
      buf[ln++] = (y - y1) * (x2 - x1) / (y2 - y1) + x1;
    }
    qsort(buf, ln, sizeof(float), compareFloat);

    for (i = 0; i + 1 < ln; i += 2) {
      i32 startX = firound(buf[i]);
      i32 endX = firound(buf[i + 1]);
      for (x = startX; x <= endX; x++) {
        setPixel(canvas, x, y, color);
      }
    }
  }

  free(buf);
  return UTRUE;
}

static ubool copyScaledCanvas(
    ObjCanvas *target,
    ObjCanvas *source,
    const Rect *srcRect,
    const Rect *dstRect,
    ubool flipX,
    ubool flipY) {
  i32 sx0 = srcRect ? (double)srcRect->minX : 0;
  i32 sy0 = srcRect ? (double)srcRect->minY : 0;
  i32 sw = srcRect ? (double)srcRect->width : WIDTHOF(source);
  i32 sh = srcRect ? (double)srcRect->height : HEIGHTOF(source);
  i32 dx0 = dstRect ? (double)dstRect->minX : 0;
  i32 dy0 = dstRect ? (double)dstRect->minY : 0;
  i32 dw = dstRect ? (double)dstRect->width : WIDTHOF(target);
  i32 dh = dstRect ? (double)dstRect->height : HEIGHTOF(target);
  i32 j, i;
  if (target == source) {
    /* TODO: allow some cases when the regions do not overlap */
    runtimeError("An Canvas may not copy into itself");
    return UFALSE;
  }
  for (j = dy0 < 0 ? -dy0 : 0; j < dh; j++) {
    i32 dy = dy0 + j;
    i32 sy = sy0 + (flipY ? (dh - 1 - j) : j) * sh / dh;
    if (dy >= HEIGHTOF(target)) {
      break;
    }
    if (dy < 0 || sy < 0 || sy >= HEIGHTOF(source)) {
      continue;
    }
    for (i = dx0 < 0 ? -dx0 : 0; i < dw; i++) {
      i32 dx = dx0 + i;
      i32 sx = sx0 + (flipX ? (dw - 1 - i) : i) * sw / dw;
      if (dx >= WIDTHOF(target)) {
        break;
      }
      if (dx < 0 || sx < 0 || sx >= WIDTHOF(source)) {
        continue;
      }
      PIXELSOF(target)[dy * WIDTHOF(target) + dx] = PIXELSOF(source)[sy * WIDTHOF(source) + sx];
    }
  }
  return UTRUE;
}

static ubool copyCanvas(
    ObjCanvas *target,
    ObjCanvas *source,
    const Rect *srcRect,
    i32 dstX, i32 dstY,
    ubool flipX,
    ubool flipY) {
  Rect dstRect;
  dstRect.minX = dstX;
  dstRect.minY = dstY;
  dstRect.width = srcRect ? srcRect->width : WIDTHOF(source);
  dstRect.height = srcRect ? srcRect->height : HEIGHTOF(source);
  return copyScaledCanvas(target, source, srcRect, &dstRect, flipX, flipY);
}

static ubool implCanvasStaticCall(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = newCanvas(AS_IMAGE(args[0]));
  *out = CANVAS_VAL(canvas);
  return UTRUE;
}

static TypePattern argsCanvasStaticCall[] = {
  { TYPE_PATTERN_NATIVE, &descriptorImage },
};

static CFunction funcCanvasStaticCall = {
  implCanvasStaticCall, "__call__", 1, 0, argsCanvasStaticCall
};

static ubool implCanvasGetattr(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.widthString) {
    *out = NUMBER_VAL(WIDTHOF(canvas));
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(HEIGHTOF(canvas));
  } else if (name == imageString) {
    *out = IMAGE_VAL(canvas->image);
  } else {
    runtimeError("Field %s not found on Canvas", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcCanvasGetattr = {
  implCanvasGetattr, "__getattr__", 1, 0, argsStrings,
};

static ubool implCanvasGet(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  size_t row = AS_INDEX(args[0], HEIGHTOF(canvas));
  size_t column = AS_INDEX(args[1], WIDTHOF(canvas));
  *out = COLOR_VAL(PIXELSOF(canvas)[row * WIDTHOF(canvas) + column]);
  return UTRUE;
}

static CFunction funcCanvasGet = {
  implCanvasGet, "get", 2, 0, argsNumbers,
};

static ubool implCanvasSet(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  size_t row = AS_INDEX(args[0], HEIGHTOF(canvas));
  size_t column = AS_INDEX(args[1], WIDTHOF(canvas));
  Color color = AS_COLOR(args[2]);
  PIXELSOF(canvas)[row * WIDTHOF(canvas) + column] = color;
  return UTRUE;
}

static TypePattern argsCanvasSet[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasSet = {
  implCanvasSet, "set", 3, 0, argsCanvasSet
};

static ubool implCanvasGetPen(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  if (!canvas->pen) {
    if (!canvasNewPen(canvas)) {
      return UFALSE;
    }
  }
  *out = PEN_VAL(canvas->pen);
  return UTRUE;
}

static CFunction funcCanvasGetPen = { implCanvasGetPen, "getPen" };

static ubool implCanvasFill(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  Color color = argc > 0 && !IS_NIL(args[0]) ? AS_COLOR(args[0]) : newColor(0, 0, 0, 0);
  size_t pixc = WIDTHOF(canvas) * HEIGHTOF(canvas), i;
  for (i = 0; i < pixc; i++) {
    PIXELSOF(canvas)[i] = color;
  }
  return UTRUE;
}

static TypePattern argsCanvasFill[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasFill = {
  implCanvasFill, "fill", 1, 0, argsCanvasFill,
};

static ubool implCanvasDrawLine(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  Vector start = AS_VECTOR(args[0]);
  Vector end = AS_VECTOR(args[1]);
  Color color = AS_COLOR(args[2]);
  return drawLine(canvas, start.x, start.y, end.x, end.y, color);
}

static TypePattern argsCanvasDrawLine[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasDrawLine = {
  implCanvasDrawLine, "drawLine", 3, 0, argsCanvasDrawLine,
};

static ubool implCanvasFillRect(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  ObjRect *rect = AS_RECT(args[0]);
  Color color = AS_COLOR(args[1]);
  return drawRect(canvas, &rect->handle, DRAW_STYLE_FILL, color);
}

static TypePattern argsCanvasFillRect[] = {
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasFillRect = {
  implCanvasFillRect, "fillRect", 2, 0, argsCanvasFillRect,
};

static ubool implCanvasStrokeRect(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  ObjRect *rect = AS_RECT(args[0]);
  Color color = AS_COLOR(args[1]);
  return drawRect(canvas, &rect->handle, DRAW_STYLE_OUTLINE, color);
}

static TypePattern argsCanvasStrokeRect[] = {
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasStrokeRect = {
  implCanvasStrokeRect, "strokeRect", 2, 0, argsCanvasStrokeRect,
};

static ubool implCanvasFillPolygon(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  ObjList *points = AS_LIST(args[0]);
  Color color = AS_COLOR(args[1]);
  size_t i;
  ubool status;
  float *coordinates = (float*)malloc(sizeof(float) * points->length * 2);
  for (i = 0; i < points->length; i++) {
    Vector point = AS_VECTOR(points->buffer[i]);
    coordinates[2 * i + 0] = point.x;
    coordinates[2 * i + 1] = point.y;
  }
  status = fillPolygon(canvas, points->length, coordinates, color);
  free(coordinates);
  return status;
}

static TypePattern argsCanvasFillPolygon[] = {
  { TYPE_PATTERN_LIST_VECTOR },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasFillPolygon = {
  implCanvasFillPolygon, "fillPolygon", 2, 0, argsCanvasFillPolygon
};

static ubool implCanvasFillCircle(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  Vector center = AS_VECTOR(args[0]);
  double radius = AS_NUMBER(args[1]);
  Color color = AS_COLOR(args[2]);
  return drawEllipse(canvas, center.x, center.y, radius, radius, DRAW_STYLE_FILL, color);
}

static TypePattern argsCanvasFillCircle[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasFillCircle = {
  implCanvasFillCircle, "fillCircle", 3, 0, argsCanvasFillCircle,
};

static ubool implCanvasStrokeCircle(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  Vector center = AS_VECTOR(args[0]);
  double radius = AS_NUMBER(args[1]);
  Color color = AS_COLOR(args[2]);
  return drawEllipse(canvas, center.x, center.y, radius, radius, DRAW_STYLE_OUTLINE, color);
}

static TypePattern argsCanvasStrokeCircle[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasStrokeCircle = {
  implCanvasStrokeCircle, "strokeCircle", 3, 0, argsCanvasStrokeCircle,
};

static ubool implCanvasFillOval(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  ObjRect *rect = AS_RECT(args[0]);
  Color color = AS_COLOR(args[1]);
  double cx = rect->handle.minX + rect->handle.width / 2;
  double cy = rect->handle.minY + rect->handle.height / 2;
  double a = rect->handle.width / 2;
  double b = rect->handle.height / 2;
  return drawEllipse(canvas, cx, cy, a, b, DRAW_STYLE_FILL, color);
}

static TypePattern argsCanvasFillOval[] = {
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasFillOval = {
  implCanvasFillOval, "fillOval", 2, 0, argsCanvasFillOval,
};

static ubool implCanvasStrokeOval(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  ObjRect *rect = AS_RECT(args[0]);
  Color color = AS_COLOR(args[1]);
  double cx = rect->handle.minX + rect->handle.width / 2;
  double cy = rect->handle.minY + rect->handle.height / 2;
  double a = rect->handle.width / 2;
  double b = rect->handle.height / 2;
  return drawEllipse(canvas, cx, cy, a, b, DRAW_STYLE_OUTLINE, color);
}

static TypePattern argsCanvasStrokeOval[] = {
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcCanvasStrokeOval = {
  implCanvasStrokeOval, "strokeOval", 2, 0, argsCanvasStrokeOval,
};

static ubool implCanvasCopy(i16 argc, Value *args, Value *out) {
  ObjCanvas *target = AS_CANVAS(args[-1]);
  ObjCanvas *source = AS_CANVAS(args[0]);
  Vector dst = AS_VECTOR(args[1]);
  Rect *srcRect = argc > 2 && !IS_NIL(args[2]) ? &AS_RECT(args[2])->handle : NULL;
  ubool flipX = argc > 3 && !IS_NIL(args[3]) ? AS_BOOL(args[3]) : UFALSE;
  ubool flipY = argc > 4 && !IS_NIL(args[4]) ? AS_BOOL(args[4]) : UFALSE;
  return copyCanvas(target, source, srcRect, dst.x, dst.y, flipX, flipY);
}

static TypePattern argsCanvasCopy[] = {
  { TYPE_PATTERN_NATIVE, &descriptorCanvas },
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_BOOL },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcCanvasCopy = {
  implCanvasCopy, "copy", 2, 5, argsCanvasCopy,
};

static ubool implCanvasCopyScaled(i16 argc, Value *args, Value *out) {
  ObjCanvas *target = AS_CANVAS(args[-1]);
  ObjCanvas *source = AS_CANVAS(args[0]);
  Rect *srcRect = argc > 1 && !IS_NIL(args[1]) ? &AS_RECT(args[1])->handle : NULL;
  Rect *dstRect = argc > 2 && !IS_NIL(args[2]) ? &AS_RECT(args[2])->handle : NULL;
  ubool flipX = argc > 3 && !IS_NIL(args[3]) ? AS_BOOL(args[3]) : UFALSE;
  ubool flipY = argc > 4 && !IS_NIL(args[4]) ? AS_BOOL(args[4]) : UFALSE;
  return copyScaledCanvas(target, source, srcRect, dstRect, flipX, flipY);
}

static TypePattern argsCanvasCopyScaled[] = {
  { TYPE_PATTERN_NATIVE, &descriptorCanvas },
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_NATIVE, &descriptorRect },
  { TYPE_PATTERN_BOOL },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcCanvasCopyScaled = {
  implCanvasCopyScaled, "copyScaled", 1, 5, argsCanvasCopyScaled,
};

static ubool implCanvasSaveToPNG(i16 argc, Value *args, Value *out) {
  ObjCanvas *canvas = AS_CANVAS(args[-1]);
  ObjDataSink *sink = AS_DATA_SINK(args[0]);
  return savePNGImage(sink, canvas->image);
}

static TypePattern argsCanvasSaveToPNG[] = {
  { TYPE_PATTERN_NATIVE, &descriptorDataSink},
};

static CFunction funcCanvasSaveToPNG = {
  implCanvasSaveToPNG, "saveToPNG", 1, 0, argsCanvasSaveToPNG
};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *staticCanvasMethods[] = {
    &funcCanvasStaticCall,
    NULL,
  };
  CFunction *canvasMethods[] = {
    &funcCanvasGetattr,
    &funcCanvasGet,
    &funcCanvasSet,
    &funcCanvasGetPen,
    &funcCanvasFill,
    &funcCanvasDrawLine,
    &funcCanvasFillRect,
    &funcCanvasStrokeRect,
    &funcCanvasFillPolygon,
    &funcCanvasFillCircle,
    &funcCanvasStrokeCircle,
    &funcCanvasFillOval,
    &funcCanvasStrokeOval,
    &funcCanvasCopy,
    &funcCanvasCopyScaled,
    &funcCanvasSaveToPNG,
    NULL,
  };

  if (!importModuleAndPop("media.image")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.font")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.font.roboto.mono")) {
    return UFALSE;
  }

  moduleRetain(module, STRING_VAL(imageString = internCString("image")));

  newNativeClass(module, &descriptorCanvas, canvasMethods, staticCanvasMethods);

  return UTRUE;
}

static CFunction func = { impl, "media.canvas", 1 };

void addNativeModuleMediaCanvas(void) {
  addNativeModule(&func);
}
