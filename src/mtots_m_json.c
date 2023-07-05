#include "mtots_m_json.h"

#include "mtots_m_json_parse.h"
#include "mtots_m_json_write.h"

static ubool implLoads(i16 argCount, Value *args, Value *out) {
  String *str = AS_STRING(args[0]);
  JSONParseState state;
  initJSONParseState(&state, str->chars);
  if (!parseJSON(&state)) {
    return UFALSE;
  }
  *out = pop();
  return UTRUE;
}

static CFunction funcLoads = { implLoads, "loads", 1, 0, argsStrings };

static ubool implDumps(i16 argCount, Value *args, Value *out) {
  size_t len;
  char *chars;
  if (!writeJSON(args[0], &len, NULL)) {
    /* TODO: Refactor error message API */
    runtimeError("Failed to handle error");
    return UFALSE;
  }
  chars = malloc(sizeof(char) * (len + 1));
  writeJSON(args[0], NULL, chars);
  chars[len] = '\0';
  *out = STRING_VAL(internOwnedString(chars, len));
  return UTRUE;
}

static CFunction funcDumps = { implDumps, "dumps", 1 };

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
    &funcLoads,
    &funcDumps,
  };
  size_t i;

  for (i = 0; i < sizeof(functions)/sizeof(CFunction*); i++) {
    mapSetN(&module->fields, functions[i]->name, CFUNCTION_VAL(functions[i]));
  }

  return UTRUE;
}

static CFunction func = { impl, "json", 1 };

void addNativeModuleJson() {
  addNativeModule(&func);
}
