#ifndef mtots_wave_h
#define mtots_wave_h

#include "mtots_util.h"

ubool readWaveFileData(Buffer *fileData, Buffer *out);

ubool writeWaveFileData(Buffer *pcmData, Buffer *out);

#endif/*mtots_wave_h*/
