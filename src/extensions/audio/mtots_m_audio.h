#ifndef mtots_m_audio_h
#define mtots_m_audio_h

/* Native Module audio */

#include "mtots_object.h"

#define AS_AUDIO(value) ((ObjAudio*)AS_OBJ(value))
#define IS_AUDIO(value) (getNativeObjectDescriptor(value) == &descriptorAudio)

typedef struct ObjAudio {
  ObjNative obj;
  Buffer buffer;  /* 16-bit signed integer, 44.1kHz stereo */
} ObjAudio;

Value AUDIO_VAL(ObjAudio *audio);

void addNativeModuleMediaAudio(void);

extern NativeObjectDescriptor descriptorAudio;

#endif/*mtots_m_audio_h*/
