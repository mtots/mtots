#include "mtots_m_json.h"

#include "mtots_m_data.h"
#include "mtots_m_json_parse.h"
#include "mtots_m_json_write.h"

static Status implLoad(i16 argc, Value *args, Value *out) {
  ObjDataSource *src = asDataSource(args[0]);
  String *str;
  JSONParseState state;
  if (!dataSourceReadToString(src, &str)) {
    return STATUS_ERROR;
  }
  push(valString(str));
  initJSONParseState(&state, str->chars);
  if (!parseJSON(&state)) {
    return STATUS_ERROR;
  }
  pop(); /* str */
  return STATUS_OK;
}

static CFunction funcLoad = {implLoad, "load", 1, 0};

static Status implDump(i16 argc, Value *args, Value *out) {
  StringBuilder sb;
  ObjDataSink *sink = asDataSink(args[0]);
  ubool status;
  initStringBuilder(&sb);
  if (!writeJSON(args[0], &sb)) {
    freeStringBuilder(&sb);
    return STATUS_ERROR;
  }
  status = dataSinkWriteBytes(sink, (const u8 *)sb.buffer, sb.length);
  freeStringBuilder(&sb);
  return status;
}

static CFunction funcDump = {implDump, "dump", 2, 0};

static Status implLoads(i16 argCount, Value *args, Value *out) {
  String *str = asString(args[0]);
  JSONParseState state;
  initJSONParseState(&state, str->chars);
  if (!parseJSON(&state)) {
    return STATUS_ERROR;
  }
  *out = pop();
  return STATUS_OK;
}

static CFunction funcLoads = {implLoads, "loads", 1, 0};

static Status implDumps(i16 argCount, Value *args, Value *out) {
  StringBuilder sb;
  initStringBuilder(&sb);
  if (!writeJSON(args[0], &sb)) {
    freeStringBuilder(&sb);
    return STATUS_ERROR;
  }
  *out = valString(sbstring(&sb));
  freeStringBuilder(&sb);
  return STATUS_OK;
}

static CFunction funcDumps = {implDumps, "dumps", 1};

static Status impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcLoad,
      &funcDump,
      &funcLoads,
      &funcDumps,
  };
  size_t i;

  for (i = 0; i < sizeof(functions) / sizeof(CFunction *); i++) {
    mapSetN(&module->fields, functions[i]->name, valCFunction(functions[i]));
  }

  return STATUS_OK;
}

static CFunction func = {impl, "json", 1};

void addNativeModuleJson(void) {
  addNativeModule(&func);
}
