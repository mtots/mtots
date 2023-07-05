#include "mtots_m_fontrm.h"

#include "mtots_m_fontrm_data.h"

#include "mtots_vm.h"

ubool loadRobotoMonoFont(ObjFont **out) {
  /* We need to ensure that the font module is loaded */
  if (!importModuleAndPop("media.font")) {
    return UFALSE;
  }

  return newFontFromMemory(
    NIL_VAL(), robotoMonoFontData, ROBOTO_MONO_FONT_DATA_LEN, out);
}

static ubool implLoad(i16 argc, Value *args, Value *out) {
  ObjFont *font;
  if (!loadRobotoMonoFont(&font)) {
    return UFALSE;
  }
  *out = FONT_VAL(font);
  return UTRUE;
}

static CFunction funcLoad = { implLoad, "load" };

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
    &funcLoad,
    NULL,
  };

  if (!importModuleAndPop("media.font")) {
    return UFALSE;
  }

  moduleAddFunctions(module, functions);

  return UTRUE;
}

static CFunction func = { impl, "media.font.roboto.mono", 1 };

void addNativeModuleMediaFontRobotoMono() {
  addNativeModule(&func);
}
