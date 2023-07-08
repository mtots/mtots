#include "mtots_m_audio.h"

#include "mtots_vm.h"
#include "mtots_wave.h"

#include <stdlib.h>

/* 16-bits per channel sample, so 2 bytes */
#define BYTES_PER_CHANNEL_SAMPLE 2

/* 2 channels (i.e. stereo), so 2 times BYTES_PER_CHANNEL_SAMPLE */
#define BYTES_PER_SAMPLE (BYTES_PER_CHANNEL_SAMPLE * 2)

static void freeAudio(ObjNative *n) {
  ObjAudio *audio = (ObjAudio*)n;
  freeBuffer(&audio->buffer);
}

NativeObjectDescriptor descriptorAudio = {
  nopBlacken, freeAudio, sizeof(ObjAudio), "Audio"
};

Value AUDIO_VAL(ObjAudio *audio) {
  return OBJ_VAL_EXPLICIT((Obj*)audio);
}

static size_t getSampleCount(ObjAudio *audio) {
  /* There are 4 bytes per sample (2-bytes per channel-sample, 2 channels) */
  return audio->buffer.length / BYTES_PER_SAMPLE;
}

static ObjAudio *newAudio(void) {
  ObjAudio *audio = NEW_NATIVE(ObjAudio, &descriptorAudio);
  initBuffer(&audio->buffer);
  return audio;
}

static ubool implAudioStaticFromSampleCount(i16 argc, Value *args, Value *out) {
  size_t sampleCount = AS_SIZE(args[0]);
  ObjAudio *audio = newAudio();
  bufferSetLength(&audio->buffer, sampleCount * BYTES_PER_SAMPLE);
  *out = AUDIO_VAL(audio);
  return UTRUE;
}

static CFunction funcAudioStaticFromSampleCount = {
  implAudioStaticFromSampleCount, "fromSampleCount", 1, 0, argsNumbers
};

static ubool implAudioStaticFromWaveFile(i16 argc, Value *args, Value *out) {
  String *path = AS_STRING(args[0]);
  size_t fileSize;
  void *buffer;
  ObjAudio *audio;
  Buffer fileData;
  if (!readFile(path->chars, &buffer, &fileSize)) {
    return UFALSE;
  }
  initBufferWithExternalData(&fileData, (u8*)buffer, fileSize);
  audio = newAudio();
  if (!readWaveFileData(&fileData, &audio->buffer)) {
    freeBuffer(&fileData);
    free(buffer);
    return UFALSE;
  }
  free(buffer);
  *out = AUDIO_VAL(audio);
  return UTRUE;
}

static CFunction funcAudioStaticFromWaveFile = {
  implAudioStaticFromWaveFile, "fromWaveFile", 1, 0, argsStrings
};

static ubool implAudioLen(i16 argc, Value *args, Value *out) {
  ObjAudio *audio = AS_AUDIO(args[-1]);
  *out = NUMBER_VAL(getSampleCount(audio));
  return UTRUE;
}

static CFunction funcAudioLen = { implAudioLen, "__len__" };

static ubool implAudioGet(i16 argc, Value *args, Value *out) {
  ObjAudio *audio = AS_AUDIO(args[-1]);
  size_t sampleID = AS_INDEX(args[0], getSampleCount(audio));
  size_t channelID = AS_INDEX(args[1], 2);
  size_t offset = sampleID * BYTES_PER_SAMPLE + channelID * BYTES_PER_CHANNEL_SAMPLE;
  *out = NUMBER_VAL(bufferGetI16(&audio->buffer, offset));
  return UTRUE;
}

static CFunction funcAudioGet = { implAudioGet, "get", 2, 0, argsNumbers };

static ubool implAudioSet(i16 argc, Value *args, Value *out) {
  ObjAudio *audio = AS_AUDIO(args[-1]);
  size_t sampleID = AS_INDEX(args[0], getSampleCount(audio));
  size_t channelID = AS_INDEX(args[1], 2);
  size_t offset = sampleID * BYTES_PER_SAMPLE + channelID * BYTES_PER_CHANNEL_SAMPLE;
  i16 value = AS_I16(args[2]);
  bufferSetI16(&audio->buffer, offset, value);
  return UTRUE;
}

static CFunction funcAudioSet = { implAudioSet, "set", 3, 0, argsNumbers };

static ubool implAudioSaveToWaveFile(i16 argc, Value *args, Value *out) {
  ObjAudio *audio = AS_AUDIO(args[-1]);
  String *fileName = AS_STRING(args[0]);
  Buffer fileData;
  initBuffer(&fileData);
  if (!writeWaveFileData(&audio->buffer, &fileData)) {
    return UFALSE;
  }
  if (!writeFile(fileData.data, fileData.length, fileName->chars)) {
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcAudioSaveToWaveFile = {
  implAudioSaveToWaveFile, "saveToWaveFile", 1, 0, argsStrings
};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *staticAudioMethods[] = {
    &funcAudioStaticFromSampleCount,
    &funcAudioStaticFromWaveFile,
    NULL,
  };
  CFunction *audioMethods[] = {
    &funcAudioLen,
    &funcAudioGet,
    &funcAudioSet,
    &funcAudioSaveToWaveFile,
    NULL,
  };
  newNativeClass(module, &descriptorAudio, audioMethods, staticAudioMethods);
  return UTRUE;
}

static CFunction func = { impl, "media.audio", 1 };

void addNativeModuleMediaAudio(void) {
  addNativeModule(&func);
}
