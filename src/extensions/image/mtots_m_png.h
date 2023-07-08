#ifndef mtots_m_png_h
#define mtots_m_png_h

/* Native Module png */

#include "mtots_m_image.h"
#include "mtots_m_data.h"

ubool loadPNGImage(ObjDataSource *ds, ObjImage **out);
ubool savePNGImage(ObjImage *image, ObjDataSink *ds);

void addNativeModuleMediaPNG(void);

#endif/*mtots_m_png_h*/
