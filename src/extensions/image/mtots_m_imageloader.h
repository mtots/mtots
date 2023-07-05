#ifndef mtots_m_imageloader_h
#define mtots_m_imageloader_h

/* Native Module media.image.loader */

#include "mtots_m_image.h"
#include "mtots_m_data.h"

ubool loadImage(ObjDataSource *ds, ObjImage **out);

void addNativeModuleMediaImageLoader();

#endif/*mtots_m_imageloader_h*/
