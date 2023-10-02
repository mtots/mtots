#include "mtots_modules.h"

#include "mtots_m_bmon.h"
#include "mtots_m_data.h"
#include "mtots_m_fs.h"
#include "mtots_m_json.h"
#include "mtots_m_os.h"
#include "mtots_m_os_path.h"
#include "mtots_m_platform.h"
#include "mtots_m_random.h"
#include "mtots_m_sdl.h"
#include "mtots_m_subprocess.h"
#include "mtots_m_sys.h"

void addNativeModules(void) {
  addNativeModuleSys();
  addNativeModuleOs();
  addNativeModuleFs();
  addNativeModuleJson();
  addNativeModuleBmon();
  addNativeModuleRandom();
  addNativeModuleData();
  addNativeModuleSDL();
  addNativeModuleSubprocess();
  addNativeModulePlatform();
  addNativeModuleOsPath();
}
