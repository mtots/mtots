#ifndef mtots_m_bmon_h
#define mtots_m_bmon_h

/* Native Module bmon */
#include "mtots_value.h"

/**
 * Utility for serializing values into BMON from native code.
 */
ubool bmonDump(Value value, Buffer *out);

/**
 * BMON = Binary Mtots Object Notation.
 *
 * Kind of meant to be like a binary JSON.
 *
 * MongoDB's BSON adds a bit too many bells and whistles imo.
 *
 * The serialization and deserialization is dead simple and easier than
 * even JSON, since there's no need to worry about formatting and
 * string literal escapes.
 */
void addNativeModuleBmon(void);

#endif/*mtots_m_bmon_h*/
