#include "mtots_m_json.h"

#include "mtots_m_data.h"
#include "mtots_m_json_parse.h"
#include "mtots_m_json_write.h"

static ubool implLoad(i16 argc, Value *args, Value *out) {
  ObjDataSource *src = AS_DATA_SOURCE(args[0]);
  String *str;
  JSONParseState state;
  if (!dataSourceReadToString(src, &str)) {
    return UFALSE;
  }
  push(STRING_VAL(str));
  initJSONParseState(&state, str->chars);
  if (!parseJSON(&state)) {
    return UFALSE;
  }
  pop(); /* str */
  return UTRUE;
}

static TypePattern argsLoad[] = {
    {TYPE_PATTERN_NATIVE, &descriptorDataSource},
};

static CFunction funcLoad = {
    implLoad, "load", 1, 0, argsLoad};

static ubool implDump(i16 argc, Value *args, Value *out) {
  ObjDataSink *sink = AS_DATA_SINK(args[0]);
  size_t len;
  char *chars;
  ubool status;
  if (!writeJSON(args[0], &len, NULL)) {
    /* TODO: Refactor error message API */
    runtimeError("Failed to handle error");
    return UFALSE;
  }
  chars = malloc(sizeof(char) * (len + 1));
  writeJSON(args[0], NULL, chars);
  chars[len] = '\0';
  status = dataSinkWriteBytes(sink, (const u8 *)chars, len);
  free(chars);
  return status;
}

static TypePattern argsDump[] = {
    {TYPE_PATTERN_ANY},
    {TYPE_PATTERN_NATIVE, &descriptorDataSink},
};

static CFunction funcDump = {
    implDump, "dump", 2, 0, argsDump};

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

static CFunction funcLoads = {implLoads, "loads", 1, 0, argsStrings};

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

static CFunction funcDumps = {implDumps, "dumps", 1};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
      &funcLoad,
      &funcDump,
      &funcLoads,
      &funcDumps,
  };
  size_t i;

  for (i = 0; i < sizeof(functions) / sizeof(CFunction *); i++) {
    mapSetN(&module->fields, functions[i]->name, CFUNCTION_VAL(functions[i]));
  }

  return UTRUE;
}

static CFunction func = {impl, "json", 1};

void addNativeModuleJson(void) {
  addNativeModule(&func);
}
