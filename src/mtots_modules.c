#include "mtots_modules.h"

#include "mtots_m_bmon.h"
#include "mtots_m_data.h"
#include "mtots_m_fs.h"
#include "mtots_m_json.h"
#include "mtots_m_os.h"
#include "mtots_m_random.h"
#include "mtots_m_sys.h"

#if MTOTS_ENABLE_GG
#include "mtots_m_gg.h"
#endif

#if MTOTS_ENABLE_AUDIO
#include "mtots_m_audio.h"
#endif

#if MTOTS_ENABLE_IMAGE
#include "mtots_m_image.h"
#include "mtots_m_imageloader.h"
#include "mtots_m_png.h"
#endif

#if MTOTS_ENABLE_CANVAS
#include "mtots_m_canvas.h"
#endif

#if MTOTS_ENABLE_FONT
#include "mtots_m_font.h"
#endif

#if MTOTS_ENABLE_FONT_ROBOTO_MONO
#include "mtots_m_fontrm.h"
#endif

#if MTOTS_ENABLE_PACO
#include "mtots_m_paco.h"
#endif

void addNativeModules(void) {
  addNativeModuleSys();
  addNativeModuleOs();
  addNativeModuleFs();
  addNativeModuleJson();
  addNativeModuleBmon();
  addNativeModuleRandom();
  addNativeModuleData();

#if MTOTS_ENABLE_GG
  addNativeModuleGG();
#endif

#if MTOTS_ENABLE_AUDIO
  addNativeModuleMediaAudio();
#endif

#if MTOTS_ENABLE_IMAGE
  addNativeModuleMediaImage();
  addNativeModuleMediaPNG();
  addNativeModuleMediaImageLoader();
#endif

#if MTOTS_ENABLE_CANVAS
  addNativeModuleMediaCanvas();
#endif

#if MTOTS_ENABLE_FONT
  addNativeModuleMediaFont();
#endif

#if MTOTS_ENABLE_FONT_ROBOTO_MONO
  addNativeModuleMediaFontRobotoMono();
#endif

#if MTOTS_ENABLE_PACO
  addNativeModulePaco();
#endif
}
