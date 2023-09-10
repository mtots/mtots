#ifndef mtots_m_json_write_h
#define mtots_m_json_write_h

#include <stdio.h>
#include <string.h>

#include "mtots_vm.h"

static ubool writeJSON(Value value, size_t *outLen, char *out) {
  if (isNil(value)) {
    if (outLen) *outLen = strlen("null");
    if (out) strcpy(out, "null");
    return STATUS_OK;
  }
  if (isBool(value)) {
    if (value.as.boolean) {
      if (outLen) *outLen = strlen("true");
      if (out) strcpy(out, "true");
    } else {
      if (outLen) *outLen = strlen("false");
      if (out) strcpy(out, "false");
    }
    return STATUS_OK;
  }
  if (isNumber(value)) {
    double x = value.as.number;
    char buffer[32];
    size_t i, len = snprintf(buffer, 32, "%f", x);
    ubool hasDot = UFALSE;
    if (len + 1 > 32) {
      /* a float required more than 31 characters */
      panic("internal error in json.dumps()");
    }
    for (i = 0; i < len; i++) {
      if (buffer[i] == '.') {
        hasDot = UTRUE;
        break;
      }
    }
    if (hasDot) {
      while (len > 0 && buffer[len - 1] == '0') {
        buffer[--len] = '\0';
      }
      if (len > 0 && buffer[len - 1] == '.') {
        buffer[--len] = '\0';
      }
    }
    if (outLen) *outLen = len;
    if (out) memcpy(out, buffer, len);
    return STATUS_OK;
  }
  if (isString(value)) {
    String *str = value.as.string;
    size_t len;
    StringEscapeOptions opts;
    initStringEscapeOptions(&opts);
    opts.jsonSafe = UTRUE;
    if (!escapeString(str->chars, str->byteLength, &opts, &len, out ? out + 1 : NULL)) {
      return STATUS_ERROR;
    }
    if (outLen) *outLen = len + 2; /* open and close quotes */
    if (out) {
      out[0] = '"';
      out[len + 1] = '"';
      out[len + 2] = '\0';
    }
    return STATUS_OK;
  }
  if (isList(value)) {
    ObjList *list = AS_LIST_UNSAFE(value);
    size_t i, len = 0;
    len++;
    if (out) *out++ = '[';
    for (i = 0; i < list->length; i++) {
      size_t itemLen;
      if (i > 0) {
        len++;
        if (out) *out++ = ',';
      }
      if (!writeJSON(list->buffer[i], &itemLen, out)) {
        if (outLen) *outLen = itemLen;
        return STATUS_ERROR;
      }
      len += itemLen;
      if (out) out += itemLen;
    }
    len++;
    if (out) *out++ = ']';
    if (outLen) *outLen = len;
    return STATUS_OK;
  }
  if (isDict(value)) {
    ObjDict *dict = AS_DICT_UNSAFE(value);
    size_t len = 0;
    MapIterator di;
    MapEntry *entry;
    ubool first = UTRUE;
    len++;
    if (out) *out++ = '{';
    initMapIterator(&di, &dict->map);
    while (mapIteratorNext(&di, &entry)) {
      size_t keyLen, valueLen;
      if (!first) {
        len++;
        if (out) *out++ = ',';
      }
      first = UFALSE;
      if (!writeJSON(entry->key, &keyLen, out)) {
        return STATUS_ERROR;
      }
      len += keyLen;
      if (out) out += keyLen;
      len++;
      if (out) *out++ = ':';
      if (!writeJSON(entry->value, &valueLen, out)) {
        return STATUS_ERROR;
      }
      len += valueLen;
      if (out) out += valueLen;
    }
    len++;
    if (out) *out++ = '}';
    if (outLen) *outLen = len;
    return STATUS_OK;
  }
  runtimeError("Cannot convert %s to JSON", getKindName(value));
  return STATUS_ERROR;
}

#endif /*mtots_m_json_write_h*/
