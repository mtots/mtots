#ifndef mtots_m_json_write_h
#define mtots_m_json_write_h

#include <stdio.h>
#include <string.h>

#include "mtots_vm.h"

static Status writeJSON(Value value, StringBuilder *out) {
  if (isNil(value)) {
    sbputstr(out, "null");
    return STATUS_OK;
  }
  if (isBool(value)) {
    sbputstr(out, value.as.boolean ? "true" : "false");
    return STATUS_OK;
  }
  if (isNumber(value)) {
    sbputnumber(out, value.as.number);
    return STATUS_OK;
  }
  if (isString(value)) {
    String *string = value.as.string;
    StringEscapeOptions opts;
    initStringEscapeOptions(&opts);
    opts.jsonSafe = UTRUE;
    sbputchar(out, '"');
    if (!escapeString(out, string->chars, string->byteLength, &opts)) {
      return STATUS_ERROR;
    }
    sbputchar(out, '"');
    return STATUS_OK;
  }
  if (isList(value)) {
    ObjList *list = AS_LIST_UNSAFE(value);
    size_t i;
    sbputchar(out, '[');
    for (i = 0; i < list->length; i++) {
      if (i > 0) {
        sbputchar(out, ',');
      }
      if (!writeJSON(list->buffer[i], out)) {
        return STATUS_ERROR;
      }
    }
    sbputchar(out, ']');
    return STATUS_OK;
  }
  if (isDict(value)) {
    ObjDict *dict = AS_DICT_UNSAFE(value);
    MapIterator iter;
    MapEntry *entry;
    ubool first = UTRUE;
    sbputchar(out, '{');
    initMapIterator(&iter, &dict->map);
    while (mapIteratorNext(&iter, &entry)) {
      if (!first) {
        sbputchar(out, ',');
      }
      first = UFALSE;
      if (!writeJSON(entry->key, out)) {
        return STATUS_ERROR;
      }
      sbputchar(out, ':');
      if (!writeJSON(entry->value, out)) {
        return STATUS_ERROR;
      }
    }
    sbputchar(out, '}');
    return STATUS_OK;
  }
  runtimeError("Cannot convert %s to JSON", getKindName(value));
  return STATUS_ERROR;
}

#endif /*mtots_m_json_write_h*/
