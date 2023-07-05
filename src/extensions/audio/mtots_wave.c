#include "mtots_wave.h"

#include <string.h>

/*
 * I'm using
 * https://ccrma.stanford.edu/courses/422-winter-2014/projects/WaveFormat/
 * as reference of what WAV/WAVE files look like
 */

#define WAVE_HEADER_SIZE 44
#define BYTES_PER_SAMPLE 4   /* 2 bytes (16-bits) * 2 channels = 4 */

static ubool emitWaveFileHeader(size_t pcmDataLen, Buffer *out) {
  bufferSetMinCapacity(out, WAVE_HEADER_SIZE);

  /* ChunkID */
  bputstr(out, "RIFF");

  /* ChunkSize */
  bufferAddU32(out, 36 + pcmDataLen);

  /* Format */
  bputstr(out, "WAVE");

  /* Subchunk1ID */
  bputstr(out, "fmt ");

  /* Subchunk1Size, always 16 for PCM */
  bufferAddU32(out, 16);

  /* AudioFormat (PCM = 1, linear quantization)
   * Values other than 1 indicate some sort of compression */
  bufferAddU16(out, 1);

  /* NumChannels (stereo = 2) */
  bufferAddU16(out, 2);

  /* SampleRate (we always use standard 44.1kHz) */
  bufferAddU32(out, 44100);

  /* ByteRate */
  bufferAddU32(out, 44100 * BYTES_PER_SAMPLE);

  /* BlockAlign */
  bufferAddU16(out, BYTES_PER_SAMPLE);

  /* BitsPerSample (channel-sample?)
   * we use 16-bit signed integers */
  bufferAddU16(out, 16);

  /* Subchunk2ID */
  bputstr(out, "data");

  /* Subchunk2Size */
  bufferAddU32(out, pcmDataLen);

  if (out->length != WAVE_HEADER_SIZE) {
    panic("emitWaveFileHeader: Assertion error header size");
  }

  return UTRUE;
}

ubool readWaveFileData(Buffer *fileData, Buffer *out) {
  Buffer header;
  size_t pcmDataLen = fileData->length - WAVE_HEADER_SIZE;
  if (fileData->length < WAVE_HEADER_SIZE) {
    runtimeError("readWaveFileData: File is too small to be recognizable WAVE");
    return UFALSE;
  }
  initBuffer(&header);
  if (!emitWaveFileHeader(pcmDataLen, &header)) {
    freeBuffer(&header);
    return UFALSE;
  }
  if (memcmp(header.data, fileData->data, WAVE_HEADER_SIZE) != 0) {
    freeBuffer(&header);
    runtimeError("readWaveFileData: Unrecognized file or WAVE file type");
    return UFALSE;
  }
  freeBuffer(&header);

  bufferClear(out);
  bufferSetMinCapacity(out, pcmDataLen);
  bufferAddBytes(out, fileData->data + WAVE_HEADER_SIZE, pcmDataLen);
  return UTRUE;
}

/*
 * Given a buffer of stereo signed 16-bit 44.1kHz PCM data,
 * emit bytes corresponding to the format of a WAVE file.
 */
ubool writeWaveFileData(Buffer *pcmData, Buffer *out) {
  size_t fileSize = WAVE_HEADER_SIZE + pcmData->length;
  out->byteOrder = MTOTS_LITTLE_ENDIAN;
  bufferClear(out);
  bufferSetMinCapacity(out, fileSize);
  bufferLock(out);

  if (!emitWaveFileHeader(pcmData->length, out)) {
    return UFALSE;
  }

  /* Data */
  bufferAddBytes(out, pcmData->data, pcmData->length);

  if (out->length != fileSize) {
    panic("writeWaveFileData: Assertion error file size");
  }

  return UTRUE;
}
