#include "mtots_m_gg.h"
#include "mtots_m_gg_scancode.h"
#include "mtots_m_gg_colors.h"
#include "mtots_sdl.h"

#include "mtots_vm.h"

#include "mtots_m_canvas.h"
#include "mtots_m_audio.h"
#include "mtots_m_fontrm.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#define DEFAULT_FRAMES_PER_SECOND 30
#define SCANCODE_KEY_COUNT 256

#define VOLUME_MAX                  128
#define DEFAULT_VOLUME           (0.25)
#define DEFAULT_FREQUENCY           440
#define PLAYBACK_CHANNEL_COUNT        8

#define SYNTH_CHANNEL_COUNT   8

#define SAMPLES_PER_SECOND 44100

#define MAX_CONTROLLER_COUNT     8
#define AXIS_THRESHOLD          (0.4)

#define WINDOW_FLAGS_MASK ( \
  SDL_WINDOW_FULLSCREEN|\
  SDL_WINDOW_FULLSCREEN_DESKTOP|\
  SDL_WINDOW_RESIZABLE)

/*
  * NOTE: SDL pixel format enums are counterintuitive to me.
  * SDL_PIXELFORMAT_RGBA8888 and SDL_PIXELFORMAT_ABGR8888 seem
  * to flip order based on endianness.
  *
  * Then SDL_PIXELFORMAT_RGBA32 flips between SDL_PIXELFORMAT_RGBA8888
  * and SDL_PIXELFORMAT_ABGR8888 based on endianness.
  */
#define PIXELFORMAT SDL_PIXELFORMAT_RGBA32

#define AS_WINDOW(v) ((ObjWindow*)AS_OBJ((v)))
#define AS_TEXTURE(v) ((ObjTexture*)AS_OBJ((v)))
#define AS_SPRITE_SHEET(v) ((ObjSpriteSheet*)AS_OBJ((v)))
#define AS_GEOMETRY(v) ((ObjGeometry*)AS_OBJ((v)))
#define AS_PLAYBACK_CHANNEL(v) ((ObjPlaybackChannel*)AS_OBJ((v)))
#define IS_WINDOW(v) ((getNativeObjectDescriptor((v)) == &descriptorWindow))
#define IS_TEXTURE(v) ((getNativeObjectDescriptor((v)) == &descriptorTexture))
#define IS_SPRITE_SHEET(v) ((getNativeObjectDescriptor((v)) == &descriptorSpriteSheet))
#define IS_GEOMETRY(v) ((getNativeObjectDescriptor((v)) == &descriptorGeometry))
#define IS_PLAYBACK_CHANNEL(v) ((getNativeObjectDescriptor((v)) == &descriptorPlaybackChannel))

typedef struct ObjTexture ObjTexture;
typedef struct ObjSpriteSheet ObjSpriteSheet;

typedef enum SynthWaveType {
  SYNTH_SINE
} SynthWaveType;

typedef enum Button {
  BUTTON_LEFT = 0,
  BUTTON_RIGHT = 1,
  BUTTON_UP = 2,
  BUTTON_DOWN = 3,
  BUTTON_O = 4,
  BUTTON_X = 5,
  BUTTON_START = 6
} Button;

typedef enum Query {
  QUERY_PRESSED = 0,
  QUERY_RELEASED = 1,
  QUERY_HELD = 2
} Query;

static ubool updateStreamingTexture(SDL_Texture *texture, ObjImage *image);

typedef struct ObjWindow {
  ObjNative obj;
  SDL_Window *handle;      /* SDL window*/
  Uint64 framesPerSecond;
  Uint64 tick;             /* number of fully processed frames so far */
  SDL_Renderer *renderer;
  Value onUpdate;
  Color backgroundColor;
  ObjCanvas *canvas;
  ObjTexture *canvasTexture;
  ObjMatrix *transform;
  u32 width, height;
} ObjWindow;

struct ObjTexture {
  ObjNative obj;
  SDL_Texture *handle;
  ObjWindow *window;
  u32 width, height;
  ObjImage *image;       /* for streaming textures */
};

struct ObjSpriteSheet {
  ObjNative obj;
  ObjTexture *texture;
  u32 spriteWidth, spriteHeight;
  u32 spriteWidthCount, spriteHeightCount;
  u32 spriteCount;
};

typedef struct ObjGeometry {
  ObjNative obj;
  ObjWindow *window;
  u32 vertexCount, indexCount;
  SDL_Vertex *vertices;         /* SDL vertices used for rendering */
  u32 *indices;
  ObjTexture *texture;
  Vector *vectors;              /* original points */
  ObjMatrix *transform;         /* transform on vectors to determine vertices */
} ObjGeometry;

typedef struct ControllerButtons {
  u8 state[SDL_CONTROLLER_BUTTON_MAX];
} ControllerButtons;

typedef struct ControllerAxes {
  i16 state[SDL_CONTROLLER_AXIS_MAX];
} ControllerAxes;

typedef struct Controller {
  SDL_GameController *handle;
  SDL_JoystickID id;

  /* -1 when opened but not active, index into 'controllers' when active */
  int playerIndex;

  /* State of the controller */
  ControllerButtons previousButtons;
  ControllerButtons currentButtons;
  ControllerAxes previousAxes;
  ControllerAxes currentAxes;
} Controller;

typedef struct ObjPlaybackChannel {
  ObjNative obj;
  u8 channelID;
} ObjPlaybackChannel;

typedef struct SynthConfig {
  SynthWaveType waveType;
  float userFrequency;         /* the frequency specified by the user */
  float lastNonzeroFrequency;  /* last nonzero userFrequency */
  float userVolume;            /* the volume specified by the user */
  float targetVolume;          /* usually userVolume, but always 0 if targetFrequency is 0 */
  float currentVolume;         /* the actual current volume */
  float volumeVelocity;        /* amount that volume should be changing per second */
} SynthConfig;

typedef struct PlaybackConfig {
  u32 repeats;
  float volume;
  ubool pause;
  ubool rewind;
} PlaybackConfig;

typedef struct PlaybackChannelData {
  u64 sampleCount, currentSample;
  i16 *pcm;
} PlaybackChannelData;

typedef struct PlaybackData {
  PlaybackChannelData array[PLAYBACK_CHANNEL_COUNT];
} PlaybackData;

typedef struct MixerConfig {
  SynthConfig synth[SYNTH_CHANNEL_COUNT];
  PlaybackConfig playback[PLAYBACK_CHANNEL_COUNT];
} MixerConfig;

typedef struct MusicState {
  u32 nextItemIndex;
  u32 timeout;
} MusicState;

static String *tickString;
static String *buttonString;
static String *dxString;
static String *dyString;
static String *transformString;
static const u8 *keyboardState;
static u8 keydownState[SCANCODE_KEY_COUNT];
static u8 keyupState[SCANCODE_KEY_COUNT];
static u8 keydownStack[SCANCODE_KEY_COUNT];
static u8 keyupStack[SCANCODE_KEY_COUNT];
static u16 keydownStackLen;
static u16 keyupStackLen;
static Vector mousePos;
static Vector mouseMotion;
static u32 previousMouseButtonState;
static u32 currentMouseButtonState;
static u32 openControllerCount;
static u32 activeControllerCount;
static Controller controllers[MAX_CONTROLLER_COUNT];
static const ControllerButtons zeroedOutControllerButtons;
static const ControllerAxes zeroedOutControllerAxes;
static u64 audioTick;
static MixerConfig mixerConfig;
static PlaybackData playbackData;
static SDL_mutex *mixerConfigMutex; /* for audioTick and MixerConfig */
static SDL_mutex *playbackDataMutex;   /* for audioData */
static SDL_AudioDeviceID audioDevice;
static MusicState musicStates[SYNTH_CHANNEL_COUNT];
static ObjList *musicList;
static ObjWindow *activeWindow;
static ObjModule *ggModule;

static Value WINDOW_VAL(ObjWindow *window) {
  return OBJ_VAL_EXPLICIT((Obj*)window);
}

static Value TEXTURE_VAL(ObjTexture *texture) {
  return OBJ_VAL_EXPLICIT((Obj*)texture);
}

static Value SPRITE_SHEET_VAL(ObjSpriteSheet *spriteSheet) {
  return OBJ_VAL_EXPLICIT((Obj*)spriteSheet);
}

static Value GEOMETRY_VAL(ObjGeometry *geometry) {
  return OBJ_VAL_EXPLICIT((Obj*)geometry);
}

static Value PLAYBACK_CHANNEL_VAL(ObjPlaybackChannel *ch) {
  return OBJ_VAL_EXPLICIT((Obj*)ch);
}

static void blackenWindow(ObjNative *n) {
  ObjWindow *window = (ObjWindow*)n;
  markValue(window->onUpdate);
  markObject((Obj*)window->canvas);
  markObject((Obj*)window->canvasTexture);
  markObject((Obj*)window->transform);
}

static void freeWindow(ObjNative *n) {
  ObjWindow *window = (ObjWindow*)n;
  SDL_DestroyRenderer(window->renderer);
  SDL_DestroyWindow(window->handle);
}

static void blackenTexture(ObjNative *n) {
  ObjTexture *texture = (ObjTexture*)n;
  markObject((Obj*)texture->window);
  markObject((Obj*)texture->image);
}

static void freeTexture(ObjNative *n) {
  ObjTexture *texture = (ObjTexture*)n;
  if (texture->handle) {
    SDL_DestroyTexture(texture->handle);
  }
}

static void blackenSpriteSheet(ObjNative *n) {
  ObjSpriteSheet *ss = (ObjSpriteSheet*)n;
  markObject((Obj*)ss->texture);
}

static void blackenGeometry(ObjNative *n) {
  ObjGeometry *geo = (ObjGeometry*)n;
  markObject((Obj*)geo->window);
  markObject((Obj*)geo->texture);
  markObject((Obj*)geo->transform);
}

static void freeGeometry(ObjNative *n) {
  ObjGeometry *geo = (ObjGeometry*)n;
  FREE_ARRAY(SDL_Vertex, geo->vertices, geo->vertexCount);
  FREE_ARRAY(u32, geo->indices, geo->indexCount);
  FREE_ARRAY(Vector, geo->vectors, geo->vertexCount);
}

static NativeObjectDescriptor descriptorWindow = {
  blackenWindow, freeWindow, sizeof(ObjWindow), "Window"
};

static NativeObjectDescriptor descriptorTexture = {
  blackenTexture, freeTexture, sizeof(ObjTexture), "Texture"
};

static NativeObjectDescriptor descriptorSpriteSheet = {
  blackenSpriteSheet, nopFree, sizeof(ObjSpriteSheet), "SpriteSheet"
};

static NativeObjectDescriptor descriptorGeometry = {
  blackenGeometry, freeGeometry, sizeof(ObjGeometry), "Geometry"
};

static NativeObjectDescriptor descriptorPlaybackChannel = {
  nopBlacken, nopFree, sizeof(ObjPlaybackChannel), "PlaybackChannel"
};

static void lockMixerConfigMutex(void) {
  if (SDL_LockMutex(mixerConfigMutex) != 0) {
    panic("SDL_LockMutex(mixerConfigMutex) failed");
  }
}

static void unlockMixerConfigMutex(void) {
  if (SDL_UnlockMutex(mixerConfigMutex) != 0) {
    panic("SDL_UnlockMutex(mixerConfigMutex) failed");
  }
}

static void lockPlaybackDataMutex(void) {
  if (SDL_LockMutex(playbackDataMutex) != 0) {
    panic("SDL_LockMutex(playbackDataMutex) failed");
  }
}

static void unlockPlaybackDataMutex(void) {
  if (SDL_UnlockMutex(playbackDataMutex) != 0) {
    panic("SDL_UnlockMutex(playbackDataMutex) failed");
  }
}

static void checkChannel(size_t channelIndex) {
  if (channelIndex >= PLAYBACK_CHANNEL_COUNT) {
    panic(
      "Invalid audio channel %lu (PLAYBACK_CHANNEL_COUNT=%d)",
      (unsigned long)channelIndex,
      PLAYBACK_CHANNEL_COUNT);
  }
}

static i16 clamp(double value) {
  return value <= I16_MIN ? I16_MIN :
         value >= I16_MAX ? I16_MAX : ((i16)value);
}

static double synthPCM(
    SynthWaveType waveType,
    u64 tick,
    float frequency,
    float volume) {
  switch (waveType) {
    case SYNTH_SINE:
      return sin(
        tick / (double)SAMPLES_PER_SECOND * frequency * TAU) *
        volume;
  }
  return 0;
}

static void incrSynthVolume(SynthConfig *c) {
  if (c->volumeVelocity > 0) {
    if (c->currentVolume + c->volumeVelocity < c->targetVolume) {
      c->currentVolume += c->volumeVelocity;
    } else {
      c->currentVolume = c->targetVolume;
    }
  } else if (c->volumeVelocity < 0) {
    if (c->currentVolume + c->volumeVelocity > c->targetVolume) {
      c->currentVolume += c->volumeVelocity;
    } else {
      c->currentVolume = c->targetVolume;
    }
  }
}

static void audioCallback(void *userData, Uint8 *stream, int byteLength) {
  u64 tick;
  MixerConfig config;
  u8 rewind[PLAYBACK_CHANNEL_COUNT];
  lockMixerConfigMutex();
  tick = audioTick;
  {
    /*
     * For better or for worse, we add 'volumeVelocity' logic to
     * reduce the clicking sound we can get when we abruptly change
     * the volume of a channel on and off.
     *
     * This effective adds 'release' and 'attack' times.
     *
     * For more info, see:
     * https://support.apple.com/en-gb/guide/logicpro/lgsife419620/mac
     * https://www.reddit.com/r/ableton/comments/1yig7j/
     */
    size_t i;
    for (i = 0; i < SYNTH_CHANNEL_COUNT; i++) {
      SynthConfig *c = mixerConfig.synth + i;
      float newTargetVolume;

      if (c->userFrequency) {
        c->lastNonzeroFrequency = c->userFrequency;
      }

      c->userVolume = dmin(1, dmax(0, c->userVolume));
      newTargetVolume = c->userFrequency ? c->userVolume : 0;
      if (newTargetVolume != c->targetVolume) {
        /* userVolume has been updated */
        c->targetVolume = newTargetVolume;

        /*
         * If the current volume is zero,
         *   set an attack of 0.01 seconds (i.e. reach the target volume in 0.01s)
         * otherwise,
         *   set the release to be 1/25 = 0.04 seconds
         *
         * We pick a shorter time for the attack, because if a sound takes
         * too long to start up, it can feel 'laggy'
         */
        c->volumeVelocity =
          c->currentVolume == 0 ?
            (c->targetVolume - c->currentVolume) / (SAMPLES_PER_SECOND / 100) :
            (c->targetVolume - c->currentVolume) / (SAMPLES_PER_SECOND /  25);
      }
    }
  }
  config = mixerConfig;
  {
    size_t i;
    for (i = 0; i < PLAYBACK_CHANNEL_COUNT; i++) {
      rewind[i] = config.playback[i].rewind;
      config.playback[i].rewind = UFALSE;
    }
  }
  unlockMixerConfigMutex();

  lockPlaybackDataMutex();
  /*
   * audioTick basically counts up 44100 per second starting from
   * when the audio is first processed.
   *
   * We also convert this tick count to a double when we perform
   * computations on it.
   *
   * This means we may start seeing some weirdness once
   * the tick count is so large that it no longer fits precisely
   * in the mantissa of the double precision floating point.
   *
   * With at least 50 bits, at 44100 ticks per second, we can
   * count up to over 800 years precisely.
   */
  {
    /* If the config says to rewind the channel, we rewind
     * to the beginning of the sample */
    size_t i;
    for (i = 0; i < PLAYBACK_CHANNEL_COUNT; i++) {
      if (rewind[i]) {
        playbackData.array[i].currentSample = 0;
      }
    }
  }
  {
    size_t sampleCount = ((size_t)byteLength) / 4, i;
    i16 *dat = (i16*)(void*)stream;
    /* NOTE: this (void*) trick is to avoid '-Wcast-align' warnigns
     * it should be ok in this case, as we always assume 16-bit stereo */

    for (i = 0; i < sampleCount; i++, tick++) {
      double left = 0, right = 0;
      double synthTotal = 0;
      size_t j;
      for (j = 0; j < SYNTH_CHANNEL_COUNT; j++) {
        SynthConfig *ch = mixerConfig.synth + j;
        incrSynthVolume(ch);
        if (ch->currentVolume == 0) {
          continue;
        }
        synthTotal = synthPCM(ch->waveType, tick, ch->lastNonzeroFrequency, ch->currentVolume);
      }
      for (j = 0; j < PLAYBACK_CHANNEL_COUNT; j++) {
        PlaybackConfig *ch = config.playback + j;
        PlaybackChannelData *chdat = playbackData.array + j;
        if (ch->pause) {
          continue;
        }
        if (chdat->currentSample >= chdat->sampleCount && ch->repeats) {
          chdat->currentSample = 0;
          ch->repeats--;
        }
        if (chdat->currentSample < chdat->sampleCount) {
          left   += ch->volume * chdat->pcm[chdat->currentSample * 2 + 0];
          right  += ch->volume * chdat->pcm[chdat->currentSample * 2 + 1];
          chdat->currentSample++;
        }
      }
      dat[2 * i + 0] = clamp(  left + I16_MAX * synthTotal );
      dat[2 * i + 1] = clamp( right + I16_MAX * synthTotal );
    }
  }
  unlockPlaybackDataMutex();
  lockMixerConfigMutex();
  audioTick = tick;
  unlockMixerConfigMutex();
}

static void prepareAudio(void) {
  if (!audioDevice) {
    SDL_AudioSpec spec;
    spec.freq = SAMPLES_PER_SECOND;
    spec.format = AUDIO_S16LSB;
    spec.channels = 2;
    spec.samples = 512;
    spec.callback = audioCallback;
    spec.userdata = NULL;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!audioDevice) {
      panic("SDL_OpenAudioDevice: %s", SDL_GetError());
    }
    SDL_PauseAudioDevice(audioDevice, 0);
  }
}

static ubool setMusic(size_t synthChannelID, ObjList *music) {
  checkChannel(synthChannelID);
  prepareAudio();
  musicStates[synthChannelID].nextItemIndex = 0;
  musicStates[synthChannelID].timeout = 0;
  if (music) {
    musicList->buffer[synthChannelID] = LIST_VAL(music);
  } else {
    musicList->buffer[synthChannelID] = NIL_VAL();
    mixerConfig.synth[synthChannelID].userFrequency = 0;
  }
  return UTRUE;
}

static ObjPlaybackChannel *getPlaybackChannel(size_t channelIndex) {
  static ObjPlaybackChannel *channels[PLAYBACK_CHANNEL_COUNT];
  ObjPlaybackChannel *ch;
  checkChannel(channelIndex);
  ch = channels[channelIndex];
  if (!ch) {
    ch = channels[channelIndex] = NEW_NATIVE(ObjPlaybackChannel, &descriptorPlaybackChannel);
    moduleRetain(ggModule, PLAYBACK_CHANNEL_VAL(ch));
    ch->channelID = channelIndex;
    lockMixerConfigMutex();
    mixerConfig.playback[channelIndex].pause = 0;
    mixerConfig.playback[channelIndex].repeats = 0;
    mixerConfig.playback[channelIndex].rewind = 0;
    mixerConfig.playback[channelIndex].volume = DEFAULT_VOLUME;
    unlockMixerConfigMutex();
  }
  return ch;
}

static void loadAudio(ObjAudio *audio, size_t channelIndex) {
  checkChannel(channelIndex);

  /* Set some sane defaults for the channel's config */
  lockMixerConfigMutex();
  {
    PlaybackConfig *ch = mixerConfig.playback + channelIndex;
    ch->volume = DEFAULT_VOLUME;
    ch->repeats = 0;
    ch->pause = 0;
  }
  unlockMixerConfigMutex();

  lockPlaybackDataMutex();
  {
    PlaybackChannelData *dat = playbackData.array + channelIndex;

    /* 16-bit stereo -> 4 bytes per sample */
    dat->sampleCount = audio->buffer.length / 4;

    /* set current sample to the end, so that it does not play right away */
    dat->currentSample = dat->sampleCount;

    dat->pcm = realloc(dat->pcm, audio->buffer.length);
    memcpy(dat->pcm, audio->buffer.data, audio->buffer.length);
  }
  unlockPlaybackDataMutex();
}

static void startAudio(size_t channelIndex, u32 repeats) {
  checkChannel(channelIndex);
  prepareAudio();
  lockMixerConfigMutex();
  {
    PlaybackConfig *ch = mixerConfig.playback + channelIndex;
    ch->repeats = repeats;
    ch->pause = UFALSE;
    ch->rewind = UTRUE;
  }
  unlockMixerConfigMutex();
}

static void pauseAudio(size_t channelIndex, ubool pause) {
  checkChannel(channelIndex);
  lockMixerConfigMutex();
  {
    PlaybackConfig *ch = mixerConfig.playback + channelIndex;
    ch->pause = pause;
  }
  unlockMixerConfigMutex();
}

static void setAudioVolume(size_t channelIndex, float volume) {
  checkChannel(channelIndex);
  lockMixerConfigMutex();
  {
    PlaybackConfig *ch = mixerConfig.playback + channelIndex;
    ch->volume = volume;
  }
  unlockMixerConfigMutex();
}

static void setDrawColor(ObjWindow *window, Color color) {
  SDL_SetRenderDrawColor(
    window->renderer, color.red, color.green, color.blue, color.alpha);
}

static ubool newWindow(
    const char *title,
    i32 width,
    i32 height,
    Uint64 framesPerSecond,
    u32 flags,
    ObjWindow **out) {
  SDL_Window *handle;
  SDL_Renderer *renderer;
  ObjWindow *window;
  int foundWidth, foundHeight;
  ubool gcPause;

  handle = SDL_CreateWindow(
    title,
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    width, height,
    flags&WINDOW_FLAGS_MASK);
  if (!handle) {
    return sdlError("SDL_CreateWindow");
  }

  renderer = SDL_CreateRenderer(handle, -1,
    SDL_RENDERER_ACCELERATED|
    SDL_RENDERER_PRESENTVSYNC|
    SDL_RENDERER_TARGETTEXTURE);
  if (!renderer) {
    sdlError("SDL_CreateRenderer");
    SDL_DestroyWindow(handle);
    return UFALSE;
  }

  /* NOTE: foundHeight/foundWidth might not match requested width
   * and height depending on the context - e.g. if we are in fullscreen mode.
   * To account for this, we get the actual render size here */
  if (SDL_GetRendererOutputSize(renderer, &foundWidth, &foundHeight) != 0) {
    sdlError("SDL_GetRendererOutputSize");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(handle);
    return UFALSE;
  }

  window = NEW_NATIVE(ObjWindow, &descriptorWindow);
  LOCAL_GC_PAUSE(gcPause);
  window->handle = handle;
  window->framesPerSecond = framesPerSecond;
  window->tick = 0;
  window->renderer = renderer;
  window->onUpdate = NIL_VAL();
  window->backgroundColor = newColor(162, 136, 121, 255); /* MEDIUM_GREY */
  window->canvas = NULL;
  window->canvasTexture = NULL;
  window->transform = newIdentityMatrix();
  window->width = (u32)foundWidth;
  window->height = (u32)foundHeight;
  LOCAL_GC_UNPAUSE(gcPause);

  *out = window;
  return UTRUE;
}

static void resetActiveControllers(void) {
  size_t i;
  for (i = 0; i < activeControllerCount; i++) {
    controllers[i].playerIndex = -1;
    controllers[i].previousButtons = zeroedOutControllerButtons;
    controllers[i].currentButtons = zeroedOutControllerButtons;
    controllers[i].previousAxes = zeroedOutControllerAxes;
    controllers[i].currentAxes = zeroedOutControllerAxes;
    SDL_GameControllerSetPlayerIndex(controllers[i].handle, -1);
  }
  activeControllerCount = 0;
}

static Controller *activateAndGetController(SDL_JoystickID id) {
  size_t i;
  Controller tmp;
  for (i = 0; i < openControllerCount; i++) {
    if (controllers[i].id == id) {
      break;
    }
  }

  /* If no matching controller is found, it's not found */
  if (i >= openControllerCount) {
    return NULL;
  }

  /* If controller is already active, just return it */
  if (i < activeControllerCount) {
    return controllers + i;
  }

  /* controllers are reordered so that active ones come first */
  tmp = controllers[activeControllerCount];
  controllers[activeControllerCount] = controllers[i];
  controllers[i] = tmp;

  /* Set the player index for the controller */
  controllers[activeControllerCount].playerIndex = (int)activeControllerCount;
  SDL_GameControllerSetPlayerIndex(
    controllers[activeControllerCount].handle,
    (int)activeControllerCount);

  /* zero out button state */
  controllers[activeControllerCount].previousButtons = zeroedOutControllerButtons;
  controllers[activeControllerCount].currentButtons = zeroedOutControllerButtons;
  controllers[activeControllerCount].previousAxes = zeroedOutControllerAxes;
  controllers[activeControllerCount].currentAxes = zeroedOutControllerAxes;

  return controllers + activeControllerCount++;
}

static double normalizeAxisValue(i16 value) {
  return (((double)value) + 0.5) / 32767.5;
}

static ubool testButtonController(Button btn, ControllerButtons *btns, ControllerAxes *axes) {
  switch (btn) {
    case BUTTON_LEFT:
      return btns->state[SDL_CONTROLLER_BUTTON_DPAD_LEFT] ||
        normalizeAxisValue(axes->state[SDL_CONTROLLER_AXIS_LEFTX]) <= -AXIS_THRESHOLD;
    case BUTTON_RIGHT:
      return btns->state[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] ||
        normalizeAxisValue(axes->state[SDL_CONTROLLER_AXIS_LEFTX]) >= AXIS_THRESHOLD;
    case BUTTON_UP:
      return btns->state[SDL_CONTROLLER_BUTTON_DPAD_UP] ||
        normalizeAxisValue(axes->state[SDL_CONTROLLER_AXIS_LEFTY]) <= -AXIS_THRESHOLD;
    case BUTTON_DOWN:
      return btns->state[SDL_CONTROLLER_BUTTON_DPAD_DOWN] ||
        normalizeAxisValue(axes->state[SDL_CONTROLLER_AXIS_LEFTY]) >= AXIS_THRESHOLD;
    case BUTTON_O:
      return btns->state[SDL_CONTROLLER_BUTTON_A] ||
        btns->state[SDL_CONTROLLER_BUTTON_Y];
    case BUTTON_X:
      return btns->state[SDL_CONTROLLER_BUTTON_B] ||
        btns->state[SDL_CONTROLLER_BUTTON_X];
    case BUTTON_START:
      return btns->state[SDL_CONTROLLER_BUTTON_START];
    default:
      panic("Invalid button ID %d", btn);
      return UFALSE;
  }
}

static ubool testButtonKeyboard(Button btn, const u8 *keyState) {
  switch (btn) {
    case BUTTON_LEFT:
      return !!(keyState[SDL_SCANCODE_A] || keyState[SDL_SCANCODE_LEFT]);
    case BUTTON_RIGHT:
      return !!(keyState[SDL_SCANCODE_D] || keyState[SDL_SCANCODE_RIGHT]);
    case BUTTON_UP:
      return !!(keyState[SDL_SCANCODE_W] || keyState[SDL_SCANCODE_UP]);
    case BUTTON_DOWN:
      return !!(keyState[SDL_SCANCODE_S] || keyState[SDL_SCANCODE_DOWN]);
    case BUTTON_O:
      return !!keyState[SDL_SCANCODE_Z];
    case BUTTON_X:
      return !!keyState[SDL_SCANCODE_X];
    case BUTTON_START:
      return !!(keyState[SDL_SCANCODE_ESCAPE] || keyState[SDL_SCANCODE_RETURN]);
    default:
      panic("Invalid button ID %d", btn);
      return UFALSE;
  }
}

static ubool testButtonKeyboardQuery(Button btn, u8 query) {
  switch (query) {
    case QUERY_PRESSED:   return testButtonKeyboard(btn, keydownState);
    case QUERY_RELEASED:  return testButtonKeyboard(btn, keyupState);
    case QUERY_HELD:      return testButtonKeyboard(btn, keyboardState);
    default:
      panic("Invalid query value: %d", query);
      return UFALSE;
  }
}

static ubool resolveQuery(u8 query, ubool previous, ubool current) {
  switch (query) {
    case QUERY_PRESSED:   return !(previous) &&  (current);
    case QUERY_RELEASED:  return  (previous) && !(current);
    case QUERY_HELD:      return                 (current);
    default: break;
  }
  panic("Invalid query value: %d", (int)query);
  return UFALSE;
}

static ubool queryButton(u8 playerID, Button btn, u8 query) {
  ubool previous, current;
  if (playerID == 0 && testButtonKeyboardQuery(btn, query)) {
    return UTRUE;
  }
  if (playerID >= activeControllerCount) {
    return UFALSE;
  }
  previous = testButtonController(
    btn,
    &controllers[playerID].previousButtons,
    &controllers[playerID].previousAxes);
  current = testButtonController(
    btn,
    &controllers[playerID].currentButtons,
    &controllers[playerID].currentAxes);
  return resolveQuery(query, previous, current);
}

static ubool mainLoopIteration(ObjWindow *mainWindow, ubool *quit) {
  SDL_Event event;
  memset(keydownState, 0, sizeof(keydownState));
  memset(keyupState, 0, sizeof(keyupState));
  keydownStackLen = 0;
  keyupStackLen = 0;
  mouseMotion = newVector(0, 0, 0);
  {
    size_t i;
    for (i = 0; i < MAX_CONTROLLER_COUNT; i++) {
      controllers[i].previousButtons = controllers[i].currentButtons;
      controllers[i].previousAxes = controllers[i].currentAxes;
    }
  }
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        *quit = UTRUE;
        return UTRUE;
      case SDL_KEYDOWN:
        if (event.key.keysym.scancode < SCANCODE_KEY_COUNT) {
          keydownState[event.key.keysym.scancode] = event.key.repeat ? 2 : 1;
          keydownStack[keydownStackLen++] = event.key.keysym.scancode;
        }
        break;
      case SDL_KEYUP:
        if (event.key.keysym.scancode < SCANCODE_KEY_COUNT) {
          keyupState[event.key.keysym.scancode] = event.key.repeat ? 2 : 1;
          keyupStack[keyupStackLen++] = event.key.keysym.scancode;
        }
        break;
      case SDL_MOUSEMOTION:
        mouseMotion.x = event.motion.xrel;
        mouseMotion.y = event.motion.yrel;
        mouseMotion.z = 0;
        break;
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_SIZE_CHANGED: {
            /* https://stackoverflow.com/questions/55076649
             * SDL_WINDOWEVENT_SIZE_CHANGED is emitted wherever
             * SDL_WINDOWEVENT_RESIZED is emitted,
             * but not vice versa */
            int w, h;
            if (SDL_GetRendererOutputSize(mainWindow->renderer, &w, &h) != 0) {
              return sdlError("SDL_GetRendererOutputSize");
            }
            mainWindow->width = (u32)w;
            mainWindow->height = (u32)h;

            /* The previous canvas, if any, is invalidated */
            mainWindow->canvas = NULL;
            mainWindow->canvasTexture = NULL;

            break;
          }
          default: break;
        }
        break;
      case SDL_CONTROLLERBUTTONDOWN: {
        Controller *c = activateAndGetController(event.cbutton.which);
        c->currentButtons.state[event.cbutton.button] = UTRUE;
        break;
      }
      case SDL_CONTROLLERBUTTONUP: {
        Controller *c = activateAndGetController(event.cbutton.which);
        c->currentButtons.state[event.cbutton.button] = UFALSE;
        break;
      }
      case SDL_CONTROLLERAXISMOTION: {
        Controller *c = activateAndGetController(event.cbutton.which);
        c->currentAxes.state[event.caxis.axis] = event.caxis.value;
        break;
      }
      case SDL_CONTROLLERDEVICEADDED:
        if (openControllerCount < MAX_CONTROLLER_COUNT) {
          Controller *c = controllers + openControllerCount++;
          SDL_Joystick *joy;
          c->handle = SDL_GameControllerOpen(event.cdevice.which);
          if (!c->handle) {
            return sdlError("SDL_GameControllerOpen");
          }
          joy = SDL_GameControllerGetJoystick(c->handle);
          if (!joy) {
            return sdlError("SDL_GameControllerGetJoystick");
          }
          c->id = SDL_JoystickInstanceID(joy);
          if (c->id < 0) {
            return sdlError("SDL_JoystickInstanceID");
          }
          c->playerIndex = -1;
        }
        break;
      case SDL_CONTROLLERDEVICEREMOVED: {
        Controller *c = NULL;
        size_t i;
        for (i = 0; i < openControllerCount; i++) {
          if (controllers[i].id == event.cdevice.which) {
            c = controllers + i;
            break;
          }
        }
        if (c) {
          SDL_GameControllerClose(c->handle);
          if (i < activeControllerCount) {
            activeControllerCount--;
          }
          for (; i < openControllerCount - 1; i++) {
            controllers[i] = controllers[i + 1];
          }
          openControllerCount--;
        }
        break;
      }
    }
  }
  {
    int x, y;
    previousMouseButtonState = currentMouseButtonState;
    currentMouseButtonState = SDL_GetMouseState(&x, &y);
    mousePos.x = (float)x;
    mousePos.y = (float)y;
  }
  setDrawColor(mainWindow, mainWindow->backgroundColor);
  SDL_RenderClear(mainWindow->renderer);
  if (!IS_NIL(mainWindow->onUpdate)) {
    push(mainWindow->onUpdate);
    if (!callFunction(0)) {
      return UFALSE;
    }
    pop();
  }
  if (mainWindow->canvasTexture) {
    if (!updateStreamingTexture(
        mainWindow->canvasTexture->handle,
        mainWindow->canvasTexture->image)) {
      return UFALSE;
    }
    if (SDL_RenderCopy(
        mainWindow->renderer,
        mainWindow->canvasTexture->handle,
        NULL,
        NULL) != 0) {
      return sdlError("SDL_RenderCopy");
    }
  }
  SDL_RenderPresent(mainWindow->renderer);

  {
    size_t i;
    ObjList *music;
    for (i = 0; i < SYNTH_CHANNEL_COUNT; i++) {
      Vector entry;
      if (musicStates[i].timeout) {
        musicStates[i].timeout--;
        continue;
      }
      if (IS_NIL(musicList->buffer[i])) {
        continue;
      }
      music = AS_LIST(musicList->buffer[i]);
      if (musicStates[i].nextItemIndex >= music->length) {
        lockMixerConfigMutex();
        mixerConfig.synth[i].userFrequency = 0;
        unlockMixerConfigMutex();
        musicList->buffer[i] = NIL_VAL();
        continue;
      }
      if (!IS_VECTOR(music->buffer[musicStates[i].nextItemIndex])) {
        panic(
          "Expected Vector for music entry, but got %s",
          getKindName(music->buffer[musicStates[i].nextItemIndex]));
      }
      entry = AS_VECTOR(music->buffer[musicStates[i].nextItemIndex++]);
      musicStates[i].timeout = (u32)(((float)mainWindow->framesPerSecond) * entry.x);
      lockMixerConfigMutex();
      mixerConfig.synth[i].userFrequency = entry.y;
      unlockMixerConfigMutex();
    }
  }

  mainWindow->tick++;
  return UTRUE;
}

#ifdef __EMSCRIPTEN__
static ObjWindow *mainWindowForEmscripten;
static void mainLoopIterationEmscripten(void) {
  ubool quit = UFALSE;
  if (!mainLoopIteration(mainWindowForEmscripten, &quit)) {
    panic("mainLoop: %s", getErrorString());
  }
  if (quit) {
    emscripten_cancel_main_loop();
  }
}
#endif


static ubool mainLoop(ObjWindow *mainWindow) {
#ifdef __EMSCRIPTEN__
  mainWindowForEmscripten = mainWindow;
  emscripten_set_main_loop(mainLoopIterationEmscripten, mainWindow->framesPerSecond, 1);
  return UTRUE;
#else
  Uint64 framesPerSecond = mainWindow->framesPerSecond;
  Uint64 countPerSecond = SDL_GetPerformanceFrequency();
  Uint64 countPerFrame = countPerSecond / framesPerSecond;
  ubool quit = UFALSE;
  push(WINDOW_VAL(mainWindow));
  for(;;) {
    Uint64 tick = mainWindow->tick;
    Uint64 startTime = SDL_GetPerformanceCounter(), endTime, elapsedTime;
    if (!mainLoopIteration(mainWindow, &quit)) {
      return UFALSE;
    }
    if (quit) {
      return UTRUE;
    }
    endTime = SDL_GetPerformanceCounter();
    elapsedTime = endTime - startTime;
    if (elapsedTime < countPerFrame) {
      Uint32 ms = (countPerFrame - elapsedTime) * 1000 / countPerSecond;
      SDL_Delay(ms);
    } else {
      eprintln(
        "WARNING: mainloop frame was delayed on tick %lu (%lu >= %lu, countPerSec=%lu)",
        (unsigned long)tick,
        (unsigned long)elapsedTime,
        (unsigned long)countPerFrame,
        (unsigned long)countPerSecond);
    }
  }
  return UTRUE;
#endif
}

static ubool newStaticTexture(ObjWindow *window, ObjImage *image, ObjTexture **out) {
  SDL_Surface *surface;
  SDL_Texture *handle;
  ObjTexture *texture;
  surface = SDL_CreateRGBSurfaceWithFormatFrom(
    image->pixels, image->width, image->height, 32, image->width * 4,
    PIXELFORMAT);
  if (!surface) {
    return sdlError("SDL_CreateRGBSurfaceWithFormatFrom");
  }
  handle = SDL_CreateTextureFromSurface(window->renderer, surface);
  if (!handle) {
    sdlError("SDL_CreateTextureFromSurface");
    SDL_FreeSurface(surface);
    return UFALSE;
  }
  SDL_FreeSurface(surface);
  texture = NEW_NATIVE(ObjTexture, &descriptorTexture);
  texture->handle = handle;
  texture->window = window;
  texture->width = image->width;
  texture->height = image->height;
  texture->image = NULL;
  *out = texture;
  return UTRUE;
}

static ubool newStreamingTexture(ObjWindow *window, ObjImage *image, ObjTexture **out) {
  ObjTexture *texture;
  SDL_Texture *handle;
  handle = SDL_CreateTexture(
    window->renderer,
    PIXELFORMAT,
    SDL_TEXTUREACCESS_STREAMING,
    image->width, image->height);
  if (!handle) {
    return sdlError("SDL_CreateTexture");
  }
  if (SDL_SetTextureBlendMode(handle, SDL_BLENDMODE_BLEND) != 0) {
    sdlError("SDL_SetTextureBlendMode");
    SDL_DestroyTexture(handle);
    return UFALSE;
  }
  if (!updateStreamingTexture(handle, image)) {
    SDL_DestroyTexture(handle);
    return UFALSE;
  }
  texture = NEW_NATIVE(ObjTexture, &descriptorTexture);
  texture->handle = handle;
  texture->window = window;
  texture->width = image->width;
  texture->height = image->height;
  texture->image = image;
  *out = texture;
  return UTRUE;
}

static ubool updateStreamingTexture(SDL_Texture *texture, ObjImage *image) {
  u8 *pixels;
  int pitch;
  size_t row;
  if (SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch) != 0) {
    return sdlError("SDL_LockTexture");
  }
  for (row = 0; row < image->height; row++, pixels += pitch) {
    memcpy(pixels, image->pixels + row * image->width, 4 * image->width);
  }
  SDL_UnlockTexture(texture);
  return UTRUE;
}

static ObjSpriteSheet *newSpriteSheet(ObjTexture *texture, u32 spriteWidth, u32 spriteHeight) {
  ObjSpriteSheet *ss = NEW_NATIVE(ObjSpriteSheet, &descriptorSpriteSheet);
  ss->texture = texture;
  ss->spriteWidth = spriteWidth;
  ss->spriteHeight = spriteHeight;
  ss->spriteWidthCount = texture->width / spriteWidth;
  ss->spriteHeightCount = texture->height / spriteHeight;
  ss->spriteCount = ss->spriteWidthCount * ss->spriteHeightCount;
  return ss;
}

static ubool windowSetCamera(ObjWindow *window, Vector upperLeft, Vector lowerRight) {
  float w = (float)window->width;
  float h = (float)window->height;

  /* The transformation that the camera needs to perform can be described
   * as a change of basis:
      (  upperLeft.x,  upperLeft.y, 0) => (0, 0, 0)
      ( lowerRight.x,  upperLeft.y, 0) => (W, 0, 0)
      ( lowerRight.x, lowerRight.y, 0) => (W, H, 0)
      (  upperLeft.x,  upperLeft.y, 1) => (0, 0, 1) */

  return initChangeOfBasisMatrix(
    &window->transform->handle,
    newVector(  upperLeft.x,  upperLeft.y, 0), newVector(0, 0, 0),
    newVector( lowerRight.x,  upperLeft.y, 0), newVector(w, 0, 0),
    newVector( lowerRight.x, lowerRight.y, 0), newVector(w, h, 0),
    newVector(  upperLeft.x,  upperLeft.y, 1), newVector(0, 0, 1));
}

static ubool windowNewCanvas(ObjWindow *window, size_t width, size_t height) {
  ObjImage *image;
  ubool gcPause;

  window->canvas = NULL;
  window->canvasTexture = NULL;
  image = newImage(width, height);

  LOCAL_GC_PAUSE(gcPause);

  memset(image->pixels, 0, sizeof(Color) * width * height);
  if (!newStreamingTexture(window, image, &window->canvasTexture)) {
    LOCAL_GC_UNPAUSE(gcPause);
    return UFALSE;
  }
  window->canvas = newCanvas(image);

  LOCAL_GC_UNPAUSE(gcPause);

  return UTRUE;
}

static SDL_Color toSDLColor(Color color) {
  SDL_Color sc;
  sc.r = color.red;
  sc.g = color.green;
  sc.b = color.blue;
  sc.a = color.alpha;
  return sc;
}

static ObjGeometry *newGeometry(ObjWindow *window, u32 vertexCount, u32 indexCount) {
  ObjGeometry *geo = NEW_NATIVE(ObjGeometry, &descriptorGeometry);
  ubool gcPause;
  LOCAL_GC_PAUSE(gcPause);
  geo->window = window;
  geo->vertexCount = vertexCount;
  geo->indexCount = indexCount;
  geo->vertices = ALLOCATE(SDL_Vertex, vertexCount);
  geo->indices = ALLOCATE(u32, indexCount);
  geo->texture = NULL;
  geo->vectors = ALLOCATE(Vector, vertexCount);
  geo->transform = newIdentityMatrix();
  LOCAL_GC_UNPAUSE(gcPause);
  return geo;
}

static ubool newPolygonGeometry(
    ObjWindow *window,
    ObjList *points,
    ObjList *colors,
    ObjTexture *texture,
    ObjList *textureCoordinates,
    ObjGeometry **out) {
  u32 vertexCount = points->length;
  u32 indexCount;
  size_t i;
  ObjGeometry *geo;
  if (vertexCount < 3) {
    runtimeError("Polygons require at least 3 vertices but got %d", (int)vertexCount);
    return UFALSE;
  }
  if (colors && colors->length != 1 && colors->length != vertexCount) {
    runtimeError(
      "Polygons require that the colors list length match the point list length "
      "but got %d points and %d colors", (int)vertexCount, (int)colors->length);
    return UFALSE;
  }
  if (textureCoordinates && textureCoordinates->length != vertexCount) {
    runtimeError(
      "Polygons require that the textureCoordinates list length match the point "
      "length but got %d points and %d texture coordinates", (int)vertexCount,
      (int)textureCoordinates->length);
    return UFALSE;
  }
  indexCount = (vertexCount - 2) * 3;
  geo = newGeometry(window, vertexCount, indexCount);
  geo->texture = texture;
  for (i = 0; i < vertexCount; i++) {
    Vector point;
    SDL_Color color;
    SDL_FPoint texCoord;
    if (!IS_VECTOR(points->buffer[i])) {
      runtimeError("Expected Vector but got %s", getKindName(points->buffer[i]));
      return UFALSE;
    }
    point = AS_VECTOR(points->buffer[i]);
    if (colors) {
      size_t colorIndex = colors->length == 1 ? 0 : i;
      if (!IS_COLOR(colors->buffer[colorIndex])) {
        runtimeError("Expected Color but got %s", getKindName(colors->buffer[colorIndex]));
        return UFALSE;
      }
      color = toSDLColor(AS_COLOR(colors->buffer[colorIndex]));
    } else {
      color = toSDLColor(newColor(255, 255, 255, 255));
    }
    if (textureCoordinates) {
      Vector vec;
      if (!IS_VECTOR(textureCoordinates->buffer[i])) {
        runtimeError("Expected Vector but got %s", getKindName(
          textureCoordinates->buffer[i]));
        return UFALSE;
      }
      vec = AS_VECTOR(textureCoordinates->buffer[i]);
      texCoord.x = vec.x;
      texCoord.y = vec.y;
    } else {
      texCoord.x = 0;
      texCoord.y = 0;
      if (texture) {
        switch (i) {
          case 0:
            break;
          case 1:
            texCoord.x = 1;
            break;
          case 2:
            texCoord.x = 1;
            texCoord.y = 1;
            break;
          case 3:
            texCoord.y = 1;
            break;
        }
      }
    }
    geo->vertices[i].color = color;
    geo->vertices[i].tex_coord = texCoord;
    geo->vertices[i].position.x = 0; /* position should be overwritten on blit */
    geo->vertices[i].position.y = 0;
    geo->vectors[i] = point;
  }
  for (i = 0; i < vertexCount - 2; i++) { /* triangle fan */
    geo->indices[3 * i + 0] = 0;
    geo->indices[3 * i + 1] = i + 1;
    geo->indices[3 * i + 2] = i + 2;
  }
  *out = geo;
  return UTRUE;
}

/* Update the coordinates of the geometry based on vectors and
  * the transform matrix.
  *
  * We take into account:
  *   1. the transform matrix of the window, and
  *   2. the transform matrix of the geometry */
static ubool geometryUpdateVertexPositions(ObjGeometry *geo) {
  size_t i;
  Matrix transform = geo->window->transform->handle;
  matrixIMul(&transform, &geo->transform->handle);
  for (i = 0; i < geo->vertexCount; i++) {
    Vector vec = matrixApply(&transform, geo->vectors[i]);
    geo->vertices[i].position.x = vec.x;
    geo->vertices[i].position.y = vec.y;
  }
  return UTRUE;
}

static ubool geometryBlit(ObjGeometry *geo) {
  if (!geometryUpdateVertexPositions(geo)) {
    return UFALSE;
  }

  /* Actually render the geometry */
  if (SDL_RenderGeometry(
      geo->window->renderer,
      geo->texture ? geo->texture->handle : NULL,
      geo->vertices,
      (int)geo->vertexCount,
      (const i32*)geo->indices,
      (int)geo->indexCount) != 0) {
    return sdlError("SDL_RenderGeometry");
  }

  return UTRUE;
}

static ubool implMainLoop(i16 argc, Value *args, Value *out) {
  vm.runOnFinish = NULL;
  return mainLoop(activeWindow);
}

static CFunction funcMainLoop = { implMainLoop, "mainLoop" };

static ubool implWindowStaticCall(i16 argc, Value *args, Value *out) {
  const char *title = argc > 0 ? AS_STRING(args[0])->chars : "";
  i32 width = argc > 1 ? AS_I32(args[1]) : 0;
  i32 height = argc > 2 ? AS_I32(args[2]) : 0;
  u32 framesPerSecond = argc > 3 ? AS_U32(args[3]) : DEFAULT_FRAMES_PER_SECOND;
  u32 flags = argc > 4 ? AS_U32_BITS(args[4]) : 0;
  if (activeWindow) {
    runtimeError("Only one window is supported at this time");
    return UFALSE;
  }
  if (!newWindow(title, width, height, framesPerSecond, flags, &activeWindow)) {
    return UFALSE;
  }
  vm.runOnFinish = &funcMainLoop;

  *out = WINDOW_VAL(activeWindow);
  return UTRUE;
}

static TypePattern argsWindowStaticCall[] = {
  { TYPE_PATTERN_STRING },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
};

static CFunction funcWindowStaticCall = {
  implWindowStaticCall, "__call__", 0, 5, argsWindowStaticCall,
};

static ubool implWindowStaticDefaultFPS(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(DEFAULT_FRAMES_PER_SECOND);
  return UTRUE;
}

static CFunction funcWindowStaticDefaultFPS = {
  implWindowStaticDefaultFPS, "defaultFPS",
};

static ubool implWindowStaticFlagFIXEDFULLSCREEN(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(SDL_WINDOW_FULLSCREEN);
  return UTRUE;
}

static CFunction funcWindowStaticFlagFIXEDFULLSCREEN = {
  implWindowStaticFlagFIXEDFULLSCREEN, "flagFIXEDFULLSCREEN",
};

static ubool implWindowStaticFlagFULLSCREEN(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(SDL_WINDOW_FULLSCREEN_DESKTOP);
  return UTRUE;
}

static CFunction funcWindowStaticFlagFULLSCREEN = {
  implWindowStaticFlagFULLSCREEN, "flagFULLSCREEN",
};

static ubool implWindowStaticFlagRESIZABLE(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(SDL_WINDOW_RESIZABLE);
  return UTRUE;
}

static CFunction funcWindowStaticFlagRESIZABLE = {
  implWindowStaticFlagRESIZABLE, "flagRESIZABLE",
};

static ubool implWindowGetattr(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == transformString) {
    *out = MATRIX_VAL(window->transform);
  } else if (name == vm.widthString) {
    *out = NUMBER_VAL(window->width);
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(window->height);
  } else if (name == tickString) {
    *out = NUMBER_VAL(window->tick);
  } else {
    runtimeError("Field %s not found on Window", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcWindowGetattr = {
  implWindowGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implWindowSetTitle(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  const char *title = AS_STRING(args[0])->chars;
  SDL_SetWindowTitle(window->handle, title);
  return UTRUE;
}

static CFunction funcWindowSetTitle = {
  implWindowSetTitle, "setTitle", 1, 0, argsStrings
};

static ubool implWindowSetBackgroundColor(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  Color color = AS_COLOR(args[0]);
  window->backgroundColor = color;
  return UTRUE;
}

static TypePattern argsWindowSetBackgroundColor[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcWindowSetBackgroundColor = {
  implWindowSetBackgroundColor, "setBackgroundColor", 1, 0, argsWindowSetBackgroundColor
};

static ubool implWindowOnUpdate(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onUpdate = args[0];
  return UTRUE;
}

static CFunction funcWindowOnUpdate = { implWindowOnUpdate, "onUpdate", 1 };

static ubool implWindowGetCanvas(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  if (!window->canvas) {
    int rw, rh;
    if (SDL_GetRendererOutputSize(window->renderer, &rw, &rh) != 0) {
      return sdlError("SDL_GetRendererOutputSize");
    }
    if (!windowNewCanvas(window, rw, rh)) {
      return UFALSE;
    }
  }
  *out = CANVAS_VAL(window->canvas);
  return UTRUE;
}

static CFunction funcWindowGetCanvas = { implWindowGetCanvas, "getCanvas" };

static ubool implWindowNewCanvas(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  i32 width = argc > 0 ? AS_I32(args[0]) : -1;
  i32 height = argc > 1 ? AS_I32(args[1]) : -1;
  if (width < 0 || height < 0) {
    int rw, rh;
    if (SDL_GetRendererOutputSize(window->renderer, &rw, &rh) != 0) {
      return sdlError("SDL_GetRendererOutputSize");
    }
    if (width < 0) {
      width = rw;
    }
    if (height < 0) {
      height = rh;
    }
  }
  if (!windowNewCanvas(window, width, height)) {
    return UFALSE;
  }
  *out = CANVAS_VAL(window->canvas);
  return UTRUE;
}

static CFunction funcWindowNewCanvas = {
  implWindowNewCanvas, "newCanvas", 0, 2, argsNumbers
};

static ubool implWindowClear(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  setDrawColor(window, AS_COLOR(args[0]));
  SDL_RenderClear(window->renderer);
  return UTRUE;
}

static TypePattern argsWindowClear[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcWindowClear = {
  implWindowClear, "clear", 1, 0, argsWindowClear
};

static ubool implWindowNewTexture(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  ObjImage *image = AS_IMAGE(args[0]);
  ubool streaming = argc > 1 && !IS_NIL(args[1]) ? AS_BOOL(args[1]) : UFALSE;
  ObjTexture *texture;

  if (streaming) {
    if (!newStreamingTexture(window, image, &texture)) {
      return UFALSE;
    }
  } else {
    if (!newStaticTexture(window, image, &texture)) {
      return UFALSE;
    }
  }
  *out = TEXTURE_VAL(texture);
  return UTRUE;
}

static TypePattern argsWindowNewTexture[] = {
  { TYPE_PATTERN_NATIVE, &descriptorImage },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcWindowNewTexture = {
  implWindowNewTexture, "newTexture", 1, 2, argsWindowNewTexture,
};

static ubool implWindowSetCamera(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  Vector upperLeft = AS_VECTOR(args[0]);
  Vector lowerRight = AS_VECTOR(args[1]);
  return windowSetCamera(window, upperLeft, lowerRight);
}

static TypePattern argsWindowSetCamera[] = {
  { TYPE_PATTERN_VECTOR },
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcWindowSetCamera = {
  implWindowSetCamera, "setCamera", 2, 0, argsWindowSetCamera
};

static ubool implWindowNewPolygon(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  ObjList *points = AS_LIST(args[0]);
  ObjList *colors = argc > 1 && !IS_NIL(args[1]) ? AS_LIST(args[1]) : NULL;
  ObjTexture *texture = argc > 2 && !IS_NIL(args[2]) ? AS_TEXTURE(args[2]) : NULL;
  ObjList *textureCoordinates = argc > 3 && !IS_NIL(args[3]) ? AS_LIST(args[3]) : NULL;
  ObjGeometry *geo;
  if (!newPolygonGeometry(window, points, colors, texture, textureCoordinates, &geo)) {
    return UFALSE;
  }
  *out = GEOMETRY_VAL(geo);
  return UTRUE;
}

static TypePattern argsWindowNewPolygon[] = {
  { TYPE_PATTERN_LIST },
  { TYPE_PATTERN_LIST_OR_NIL },
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorTexture },
  { TYPE_PATTERN_LIST_OR_NIL },
};

static CFunction funcWindowNewPolygon = {
  implWindowNewPolygon, "newPolygon", 1, 4, argsWindowNewPolygon
};

static ubool implWindowNewRect(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  Rect rect = AS_RECT(args[0]);
  ObjList *colors = argc > 1 && !IS_NIL(args[1]) ? AS_LIST(args[1]) : NULL;
  ObjTexture *texture = argc > 2 && !IS_NIL(args[2]) ? AS_TEXTURE(args[2]) : NULL;
  ObjList *textureCoordinates = argc > 3 && !IS_NIL(args[3]) ? AS_LIST(args[3]) : NULL;
  ObjGeometry *geo;
  ObjList *points = newList(4);
  ubool gcPause;
  float w = rect.width, h = rect.height;

  points->buffer[0] = VECTOR_VAL(newVector(rect.minX,     rect.minY,     0));
  points->buffer[1] = VECTOR_VAL(newVector(rect.minX + w, rect.minY,     0));
  points->buffer[2] = VECTOR_VAL(newVector(rect.minX + w, rect.minY + h, 0));
  points->buffer[3] = VECTOR_VAL(newVector(rect.minX,     rect.minY + h, 0));

  LOCAL_GC_PAUSE(gcPause);
  if (!newPolygonGeometry(window, points, colors, texture, textureCoordinates, &geo)) {
    LOCAL_GC_UNPAUSE(gcPause);
    return UFALSE;
  }
  LOCAL_GC_UNPAUSE(gcPause);
  *out = GEOMETRY_VAL(geo);
  return UTRUE;
}

static TypePattern argsWindowNewRect[] = {
  { TYPE_PATTERN_RECT },
  { TYPE_PATTERN_LIST_OR_NIL },
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorTexture },
  { TYPE_PATTERN_LIST_OR_NIL },
};

static CFunction funcWindowNewRect = {
  implWindowNewRect, "newRect", 1, 4, argsWindowNewRect
};

static ubool implWindowNewGeometry(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  u32 vertexCount = AS_U32(args[0]);
  u32 indexCount = AS_U32(args[1]);
  ObjGeometry *geo;
  if (vertexCount == 0 || indexCount == 0) {
    runtimeError(
      "A geometry's vertex and index counts must be non-zero (got %d and %d)",
      (int)vertexCount, (int)indexCount);
    return UFALSE;
  }
  *out = GEOMETRY_VAL(geo = newGeometry(window, vertexCount, indexCount));
  memset(geo->indices, 0, sizeof(geo->indices[0]) * geo->indexCount);
  memset(geo->vertices, 0, sizeof(geo->vertices[0]) * geo->vertexCount);
  memset(geo->vectors, 0, sizeof(geo->vectors[0]) * geo->vertexCount);
  return UTRUE;
}

static CFunction funcWindowNewGeometry = {
  implWindowNewGeometry, "newGeometry", 2, 0, argsNumbers
};

static ubool implTextureGetattr(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.widthString) {
    *out = NUMBER_VAL(texture->width);
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(texture->height);
  } else {
    runtimeError("Field %s not found on Texture", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcTextureGetattr = {
  implTextureGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implTextureBlit(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  ubool hasSrcRect = !IS_NIL(args[0]);
  Rect srcRect = hasSrcRect ? AS_RECT(args[0]) : newRect(0, 0, 0, 0);
  ubool hasDstRect = !IS_NIL(args[1]);
  Rect dstRect = hasDstRect ? AS_RECT(args[1]) : newRect(0, 0, 0, 0);
  double rotateAngleRadians = argc > 2 && !IS_NIL(args[2]) ? AS_NUMBER(args[2]) : 0;
  double rotateAngleDegrees = rotateAngleRadians * 180 / PI;
  ubool centerProvided = argc > 3 && !IS_NIL(args[3]);
  ubool flipX = argc > 4 && !IS_NIL(args[4]) ? AS_BOOL(args[4]) : UFALSE;
  ubool flipY = argc > 5 && !IS_NIL(args[5]) ? AS_BOOL(args[5]) : UFALSE;
  SDL_Rect srcSDLRect;
  SDL_Rect dstSDLRect;
  SDL_Point centerPoint;
  if (hasSrcRect) {
    srcSDLRect.x = srcRect.minX;
    srcSDLRect.y = srcRect.minY;
    srcSDLRect.w = srcRect.width;
    srcSDLRect.h = srcRect.height;
  }
  if (hasDstRect) {
    dstSDLRect.x = dstRect.minX;
    dstSDLRect.y = dstRect.minY;
    dstSDLRect.w = dstRect.width;
    dstSDLRect.h = dstRect.height;
  }
  if (centerProvided) {
    Vector center = AS_VECTOR(args[3]);
    centerPoint.x = center.x;
    centerPoint.y = center.y;
  }
  if (SDL_RenderCopyEx(
      texture->window->renderer,
      texture->handle,
      hasSrcRect ? &srcSDLRect : NULL,
      hasDstRect ? &dstSDLRect : NULL,
      rotateAngleDegrees,
      centerProvided ? &centerPoint : NULL,
      (flipX ? SDL_FLIP_HORIZONTAL : 0) |
      (flipY ? SDL_FLIP_VERTICAL : 0)) != 0) {
    return sdlError("SDL_RenderCopyEx");
  }
  return UTRUE;
}

static TypePattern argsTextureBlit[] = {
  { TYPE_PATTERN_RECT_OR_NIL },
  { TYPE_PATTERN_RECT_OR_NIL },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_VECTOR_OR_NIL },
  { TYPE_PATTERN_BOOL },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcTextureBlit = {
  implTextureBlit, "blit", 2, 0, argsTextureBlit,
};

static ubool implTextureIsStreaming(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  *out = BOOL_VAL(!!texture->image);
  return UTRUE;
}

static CFunction funcTextureIsStreaming = {
  implTextureIsStreaming, "isStreaming",
};

static ubool implTextureUpdate(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  if (!texture->image) {
    runtimeError("update() called on a non-streaming Texture");
    return UFALSE;
  }
  if (!updateStreamingTexture(texture->handle, texture->image)) {
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcTextureUpdate = { implTextureUpdate, "update" };

static ubool implTextureNewSpriteSheet(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  u32 spriteWidth = AS_U32(args[0]);
  u32 spriteHeight = AS_U32(args[1]);
  *out = SPRITE_SHEET_VAL(newSpriteSheet(texture, spriteWidth, spriteHeight));
  return UTRUE;
}

static CFunction funcTextureNewSpriteSheet = {
  implTextureNewSpriteSheet, "newSpriteSheet", 2, 0, argsNumbers };

static ubool implSpriteSheetBlit(i16 argc, Value *args, Value *out) {
  ObjSpriteSheet *ss = AS_SPRITE_SHEET(args[-1]);
  u32 spriteIndex = (u32)AS_INDEX(args[0], ss->spriteCount);
  Vector pos = AS_VECTOR(args[1]);
  Vector scale = argc > 2 && !IS_NIL(args[2]) ? AS_VECTOR(args[2]) : newVector(1, 1, 0);
  Vector flip = argc > 3 && !IS_NIL(args[3]) ? AS_VECTOR(args[3]) : newVector(0, 0, 0);
  SDL_Rect src, dst;
  src.x = (spriteIndex % ss->spriteWidthCount) * ss->spriteWidth;
  src.y = (spriteIndex / ss->spriteWidthCount) * ss->spriteHeight;
  src.w = (int)(ss->spriteWidth * scale.x);
  src.h = (int)(ss->spriteHeight * scale.y);
  dst.x = pos.x;
  dst.y = pos.y;
  dst.w = (int)(ss->spriteWidth * scale.x);
  dst.h = (int)(ss->spriteHeight * scale.y);
  if (SDL_RenderCopyEx(
      ss->texture->window->renderer, ss->texture->handle, &src, &dst, 0, NULL,
      SDL_FLIP_NONE |
      (flip.x ? SDL_FLIP_HORIZONTAL : 0) |
      (flip.y ? SDL_FLIP_VERTICAL : 0)) != 0) {
    return sdlError("SDL_RenderCopyEx");
  }
  return UTRUE;
}

static TypePattern argsSpriteSheetBlit[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcSpriteSheetBlit = {
  implSpriteSheetBlit, "blit", 2, 0, argsSpriteSheetBlit
};

static ubool implGeometryGetattr(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == transformString) {
    *out = MATRIX_VAL(geo->transform);
  } else {
    runtimeError("Field %s not found in Geometry", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcGeometryGetattr = {
  implGeometryGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implGeometryBlit(i16 argc, Value *args, Value *out) {
  return geometryBlit(AS_GEOMETRY(args[-1]));
}

static CFunction funcGeometryBlit = { implGeometryBlit, "blit" };

static ubool implGeometrySetTexture(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  ObjTexture *texture = IS_NIL(args[0]) ? NULL : AS_TEXTURE(args[0]);
  geo->texture = texture;
  return UTRUE;
}

static TypePattern argsGeometrySetTexture[] = {
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorTexture },
};

static CFunction funcGeometrySetTexture = {
  implGeometrySetTexture, "setTexture", 1, 0, argsGeometrySetTexture
};

static ubool implGeometrySetColor(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  SDL_Color color = toSDLColor(AS_COLOR(args[0]));
  u32 i;
  for (i = 0; i < geo->vertexCount; i++) {
    geo->vertices[i].color = color;
  }
  return UTRUE;
}

static TypePattern argsGeometrySetColor[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcGeometrySetColor = {
  implGeometrySetColor, "setColor", 1, 0, argsGeometrySetColor
};

static ubool implGeometryGetVertexCount(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  *out = NUMBER_VAL(geo->vertexCount);
  return UTRUE;
}

static CFunction funcGeometryGetVertexCount = {
  implGeometryGetVertexCount, "getVertexCount",
};

static ubool implGeometrySetVertexColor(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  size_t vi = AS_INDEX(args[0], (size_t)geo->vertexCount);
  geo->vertices[vi].color = toSDLColor(AS_COLOR(args[1]));
  return UTRUE;
}

static TypePattern argsGeometrySetVertexColor[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcGeometrySetVertexColor = {
  implGeometrySetVertexColor, "setVertexColor", 2, 0, argsGeometrySetVertexColor
};

static ubool implGeometrySetVertexTextureCoordinates(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  size_t vi = AS_INDEX(args[0], (size_t)geo->vertexCount);
  Vector coords = AS_VECTOR(args[1]);
  geo->vertices[vi].position.x = coords.x;
  geo->vertices[vi].position.y = coords.y;
  return UTRUE;
}

static TypePattern argsGeometrySetVertexTextureCoordinates[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcGeometrySetVertexTextureCoordinates = {
  implGeometrySetVertexTextureCoordinates, "setVertexTextureCoordinates", 2, 0,
  argsGeometrySetVertexTextureCoordinates
};

static ubool implGeometrySetVertexPosition(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  size_t vi = AS_INDEX(args[0], (size_t)geo->vertexCount);
  Vector pos = AS_VECTOR(args[1]);
  geo->vectors[vi] = pos;
  return UTRUE;
}

static TypePattern argsGeometrySetVertexPosition[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcGeometrySetVertexPosition = {
  implGeometrySetVertexPosition, "setVertexPosition", 2, 0, argsGeometrySetVertexPosition,
};

static ubool implGeometryGetIndexCount(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  *out = NUMBER_VAL(geo->indexCount);
  return UTRUE;
}

static CFunction funcGeometryGetIndexCount = {
  implGeometryGetIndexCount, "getIndexCount"
};

static ubool implGeometrySetIndex(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  size_t ii = AS_INDEX(args[0], (size_t)geo->indexCount);
  size_t vi = AS_INDEX(args[1], (size_t)geo->vertexCount);
  geo->indices[ii] = (u32)vi;
  return UTRUE;
}

static CFunction funcGeometrySetIndex = {
  implGeometrySetIndex, "setIndex", 2, 0, argsNumbers
};

static ubool implPlaybackChannelStaticGet(i16 argc, Value *args, Value *out) {
  size_t channelID = AS_INDEX(args[0], PLAYBACK_CHANNEL_COUNT);
  *out = PLAYBACK_CHANNEL_VAL(getPlaybackChannel(channelID));
  return UTRUE;
}

static CFunction funcPlaybackChannelStaticGet = {
  implPlaybackChannelStaticGet, "get", 1, 0, argsNumbers
};

static ubool implPlaybackChannelLoad(i16 argc, Value *args, Value *out) {
  ObjPlaybackChannel *ch = AS_PLAYBACK_CHANNEL(args[-1]);
  ObjAudio *audio = AS_AUDIO(args[0]);
  loadAudio(audio, ch->channelID);
  return UTRUE;
}

static TypePattern argsPlaybackChannelLoad[] = {
  { TYPE_PATTERN_NATIVE, &descriptorAudio },
};

static CFunction funcPlaybackChannelLoad = {
  implPlaybackChannelLoad, "load", 1, 0, argsPlaybackChannelLoad
};

static ubool implPlaybackChannelStart(i16 argc, Value *args, Value *out) {
  ObjPlaybackChannel *ch = AS_PLAYBACK_CHANNEL(args[-1]);
  i32 repeats = argc > 0 ? AS_I32(args[0]) : 0;
  if (repeats < 0) {
    repeats = I32_MAX;
  }
  startAudio(ch->channelID, (u32)repeats);
  return UTRUE;
}

static CFunction funcPlaybackChannelStart = {
  implPlaybackChannelStart, "start", 0, 1, argsNumbers
};

static ubool implPlaybackChannelPause(i16 argc, Value *args, Value *out) {
  ObjPlaybackChannel *ch = AS_PLAYBACK_CHANNEL(args[-1]);
  ubool pause = argc > 0 ? AS_BOOL(args[0]) : UTRUE;
  pauseAudio(ch->channelID, pause);
  return UTRUE;
}

static TypePattern argsPlaybackChannelPause[] = {
  { TYPE_PATTERN_BOOL },
};

static CFunction funcPlaybackChannelPause = {
  implPlaybackChannelPause, "pause", 0, 1, argsPlaybackChannelPause
};

static ubool implPlaybackChannelSetVolume(i16 argc, Value *args, Value *out) {
  ObjPlaybackChannel *ch = AS_PLAYBACK_CHANNEL(args[-1]);
  double volume = AS_NUMBER(args[0]);
  setAudioVolume(ch->channelID, volume);
  return UTRUE;
}

static CFunction funcPlaybackChannelSetVolume = {
  implPlaybackChannelSetVolume, "setVolume", 1, 0, argsNumbers
};

static ubool implSynth(i16 argc, Value *args, Value *out) {
  size_t channelID = AS_INDEX(args[0], SYNTH_CHANNEL_COUNT);
  lockMixerConfigMutex();
  if (argc > 1 && !IS_NIL(args[1])) {
    mixerConfig.synth[channelID].userFrequency = AS_NUMBER(args[1]);
  }
  if (argc > 2 && !IS_NIL(args[2])) {
    mixerConfig.synth[channelID].userVolume = AS_NUMBER(args[2]);
  }
  if (argc > 3 && !IS_NIL(args[3])) {
    mixerConfig.synth[channelID].waveType = AS_INDEX(args[3], 1);
  }
  unlockMixerConfigMutex();
  prepareAudio();
  return UTRUE;
}

static TypePattern argsSynth[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER_OR_NIL },
  { TYPE_PATTERN_NUMBER_OR_NIL },
  { TYPE_PATTERN_NUMBER_OR_NIL },
};

static CFunction funcSynth = {
  implSynth, "synth", 1, 4, argsSynth
};

static ubool implMusic(i16 argc, Value *args, Value *out) {
  size_t channelID = AS_INDEX(args[0], SYNTH_CHANNEL_COUNT);
  ObjList *music = IS_NIL(args[1]) ? NULL : AS_LIST(args[1]);
  return setMusic(channelID, music);
}

static TypePattern argsMusic[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_LIST_OR_NIL },
};

static CFunction funcMusic = {
  implMusic, "music", 2, 0, argsMusic
};

static ubool implKey(i16 argc, Value *args, Value *out) {
  size_t scancode = AS_INDEX(args[0], SCANCODE_KEY_COUNT);
  u32 query = argc > 1 ? AS_U32(args[1]) : 0;
  ubool allowRepeat = argc > 2 ? AS_BOOL(args[2]) : UTRUE;
  u8 result;
  switch (query) {
    case QUERY_PRESSED:   result =   keydownState[scancode]; break;
    case QUERY_RELEASED:  result =     keyupState[scancode]; break;
    case QUERY_HELD:      result =  keyboardState[scancode]; break;
    default:
      runtimeError("Invalid keyboard key query: %d", (int)query);
      return UFALSE;
  }
  *out = BOOL_VAL((allowRepeat && result) || result == 1);
  return UTRUE;
}

static TypePattern argsKey[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcKey = { implKey, "key", 1, 3, argsKey };

static ubool implGetKey(i16 argc, Value *args, Value *out) {
  u8 type = argc > 0 ? AS_U8(args[0]) : 0;
  switch (type) {
    case 0:
      *out = NUMBER_VAL(keydownStackLen ? keydownStack[--keydownStackLen] : -1);
      return UTRUE;
    case 1:
      *out = NUMBER_VAL(keyupStackLen ? keyupStack[--keyupStackLen] : -1);
      return UTRUE;
    default: break;
  }
  runtimeError(
    "Invalid getKey type, expected 0 for keydown or 1 for keyup, but got %d",
    type);
  return UFALSE;
}

static CFunction funcGetKey = { implGetKey, "getKey", 1, 0, argsNumbers };

static ubool implMousePosition(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(mousePos);
  return UTRUE;
}

static CFunction funcMousePosition = { implMousePosition, "mousePosition" };

static ubool implMouseMotion(i16 argc, Value *args, Value *out) {
  *out = VECTOR_VAL(mouseMotion);
  return UTRUE;
}

static CFunction funcMouseMotion = { implMouseMotion, "mouseMotion" };

static ubool implMouseButton(i16 argc, Value *args, Value *out) {
  u32 buttonID = AS_U32(args[0]);
  u32 query = argc > 1 ? AS_U32(args[1]) : 0;
  u32 bit;
  u32 previous = previousMouseButtonState, current = currentMouseButtonState;
  switch (buttonID) {
    case 0: bit = SDL_BUTTON(1); break;
    case 1: bit = SDL_BUTTON(2); break;
    case 2: bit = SDL_BUTTON(3); break;
    default:
      runtimeError("Invalid mouse button ID: %d", (int)buttonID);
      return UFALSE;
  }
  *out = BOOL_VAL(resolveQuery(query, previous&bit, current&bit));
  return UTRUE;
}

static CFunction funcMouseButton = { implMouseButton, "mouseButton", 1, 2, argsNumbers };

static ubool implGetActiveControllerCount(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(activeControllerCount);
  return UTRUE;
}

static CFunction funcGetActiveControllerCount = {
  implGetActiveControllerCount,
  "getActiveControllerCount"
};

static ubool implGetConnectedControllerCount(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(openControllerCount);
  return UTRUE;
}

static CFunction funcGetConnectedControllerCount = {
  implGetConnectedControllerCount,
  "getConnectedControllerCount"
};

static ubool implResetActiveControllers(i16 argc, Value *args, Value *out) {
  resetActiveControllers();
  return UTRUE;
}

static CFunction funcResetActiveControllers = {
  implResetActiveControllers, "resetActiveControllers",
};

static ubool implControllerButton(i16 argc, Value *args, Value *out) {
  u32 controllerIndex = AS_U32(args[0]);
  u32 buttonID = AS_U32(args[1]);
  u32 query = argc > 2 ? AS_U32(args[2]) : 0;
  u8 previous, current;
  if (controllerIndex >= activeControllerCount) {
    *out = BOOL_VAL(UFALSE);
    return UTRUE;
  }
  if (buttonID >= SDL_CONTROLLER_BUTTON_MAX) {
    runtimeError("gg.controllerButton(): invalid buttonID %d", (int)buttonID);
    return UFALSE;
  }
  previous = controllers[controllerIndex].previousButtons.state[buttonID];
  current = controllers[controllerIndex].currentButtons.state[buttonID];
  *out = BOOL_VAL(resolveQuery(query, previous, current));
  return UTRUE;
}

static CFunction funcControllerButton = {
  implControllerButton, "controllerButton", 2, 3, argsNumbers
};

static ubool implControllerAxis(i16 argc, Value *args, Value *out) {
  u32 controllerIndex = AS_U32(args[0]);
  u32 axisID = AS_U32(args[1]);
  if (controllerIndex >= activeControllerCount) {
    *out = NUMBER_VAL(0);
    return UTRUE;
  }
  if (axisID >= SDL_CONTROLLER_AXIS_MAX) {
    runtimeError("gg.controllerAxis(): invalid axsiID %d", (int)axisID);
    return UFALSE;
  }
  *out = NUMBER_VAL(normalizeAxisValue(
    controllers[controllerIndex].currentAxes.state[axisID]));
  return UTRUE;
}

static CFunction funcControllerAxis = {
  implControllerAxis, "controllerAxis", 2, 0, argsNumbers
};

static ubool implButton(i16 argc, Value *args, Value *out) {
  u32 playerIndex = AS_U32(args[0]);
  u32 buttonID = AS_U32(args[1]);
  u32 query = argc > 2 ? AS_U32(args[2]) : 0;
  *out = BOOL_VAL(queryButton(playerIndex, buttonID, query));
  return UTRUE;
}

static CFunction funcButton = {
  implButton, "button", 2, 3, argsNumbers
};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *windowStaticMethods[] = {
    &funcWindowStaticCall,
    &funcWindowStaticDefaultFPS,
    &funcWindowStaticFlagFIXEDFULLSCREEN,
    &funcWindowStaticFlagFULLSCREEN,
    &funcWindowStaticFlagRESIZABLE,
    NULL,
  };
  CFunction *windowMethods[] = {
    &funcWindowGetattr,
    &funcWindowSetTitle,
    &funcWindowSetBackgroundColor,
    &funcWindowOnUpdate,
    &funcWindowGetCanvas,
    &funcWindowNewCanvas,
    &funcWindowClear,
    &funcWindowNewTexture,
    &funcWindowSetCamera,
    &funcWindowNewPolygon,
    &funcWindowNewRect,
    &funcWindowNewGeometry,
    NULL,
  };
  CFunction *textureStaticMethods[] = {
    NULL,
  };
  CFunction *textureMethods[] = {
    &funcTextureGetattr,
    &funcTextureBlit,
    &funcTextureIsStreaming,
    &funcTextureUpdate,
    &funcTextureNewSpriteSheet,
    NULL,
  };
  CFunction *spriteSheetStaticMethods[] = {
    NULL,
  };
  CFunction *spriteSheetMethods[] = {
    &funcSpriteSheetBlit,
    NULL,
  };
  CFunction *geometryStaticMethods[] = {
    NULL,
  };
  CFunction *geometryMethods[] = {
    &funcGeometryGetattr,
    &funcGeometryBlit,
    &funcGeometrySetTexture,
    &funcGeometrySetColor,
    &funcGeometryGetVertexCount,
    &funcGeometrySetVertexColor,
    &funcGeometrySetVertexTextureCoordinates,
    &funcGeometrySetVertexPosition,
    &funcGeometryGetIndexCount,
    &funcGeometrySetIndex,
    NULL,
  };
  CFunction *playbackChannelStaticMethods[] = {
    &funcPlaybackChannelStaticGet,
    NULL,
  };
  CFunction *playbackChannelMethods[] = {
    &funcPlaybackChannelLoad,
    &funcPlaybackChannelStart,
    &funcPlaybackChannelPause,
    &funcPlaybackChannelSetVolume,
    NULL,
  };
  CFunction *functions[] = {
    &funcSynth,
    &funcMusic,
    &funcKey,
    &funcGetKey,
    &funcMousePosition,
    &funcMouseMotion,
    &funcMouseButton,
    &funcGetActiveControllerCount,
    &funcGetConnectedControllerCount,
    &funcResetActiveControllers,
    &funcControllerButton,
    &funcControllerAxis,
    &funcButton,
    NULL,
  };
  ubool gcPause;

  ggModule = module;

  if (!importModuleAndPop("media.canvas")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.audio")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.font")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.font.roboto.mono")) {
    return UFALSE;
  }

  LOCAL_GC_PAUSE(gcPause);

  moduleRetain(module, STRING_VAL(tickString = internCString("tick")));
  moduleRetain(module, STRING_VAL(buttonString = internCString("button")));
  moduleRetain(module, STRING_VAL(dxString = internCString("dx")));
  moduleRetain(module, STRING_VAL(dyString = internCString("dy")));
  moduleRetain(module, STRING_VAL(transformString = internCString("transform")));
  moduleRetain(module, LIST_VAL(musicList = newList(SYNTH_CHANNEL_COUNT)));

  moduleAddFunctions(module, functions);

  newNativeClass(
    module,
    &descriptorWindow,
    windowMethods,
    windowStaticMethods);

  newNativeClass(
    module,
    &descriptorTexture,
    textureMethods,
    textureStaticMethods);

  newNativeClass(
    module,
    &descriptorSpriteSheet,
    spriteSheetMethods,
    spriteSheetStaticMethods);

  newNativeClass(
    module,
    &descriptorGeometry,
    geometryMethods,
    geometryStaticMethods);

  newNativeClass(
    module,
    &descriptorPlaybackChannel,
    playbackChannelMethods,
    playbackChannelStaticMethods);

  {
    size_t i;
    for (i = 0; i < SYNTH_CHANNEL_COUNT; i++) {
      mixerConfig.synth[i].userVolume = DEFAULT_VOLUME;
      mixerConfig.synth[i].lastNonzeroFrequency = DEFAULT_FREQUENCY;
    }
  }

  {
    size_t i;
    Map map;
    initMap(&map);
    for (i = 0; scancodeEntries[i].name; i++) {
      mapSetN(&map, scancodeEntries[i].name, NUMBER_VAL(scancodeEntries[i].scancode));
    }
    mapSetN(&module->fields, "KEY", FROZEN_DICT_VAL(newFrozenDict(&map)));
    freeMap(&map);
  }

  {
    Map map;
    initMap(&map);
    mapSetN(&map, "A", NUMBER_VAL(0));
    mapSetN(&map, "B", NUMBER_VAL(1));
    mapSetN(&map, "X", NUMBER_VAL(2));
    mapSetN(&map, "Y", NUMBER_VAL(3));
    mapSetN(&map, "BACK", NUMBER_VAL(4));
    mapSetN(&map, "GUIDE", NUMBER_VAL(5));
    mapSetN(&map, "START", NUMBER_VAL(6));
    mapSetN(&map, "LEFTSTICK", NUMBER_VAL(7));
    mapSetN(&map, "RIGHTSTICK", NUMBER_VAL(8));
    mapSetN(&map, "LEFTSHOULDER", NUMBER_VAL(9));
    mapSetN(&map, "RIGHTSHOULDER", NUMBER_VAL(10));
    mapSetN(&map, "DPAD_UP", NUMBER_VAL(11));
    mapSetN(&map, "DPAD_DOWN", NUMBER_VAL(12));
    mapSetN(&map, "DPAD_LEFT", NUMBER_VAL(13));
    mapSetN(&map, "DPAD_RIGHT", NUMBER_VAL(14));
    mapSetN(&map, "MISC1", NUMBER_VAL(15));
    mapSetN(&map, "PADDLE1", NUMBER_VAL(16));
    mapSetN(&map, "PADDLE2", NUMBER_VAL(17));
    mapSetN(&map, "PADDLE3", NUMBER_VAL(18));
    mapSetN(&map, "PADDLE4", NUMBER_VAL(19));
    mapSetN(&map, "TOUCHPAD", NUMBER_VAL(20));
    mapSetN(&module->fields, "CONTROLLER_BUTTON", FROZEN_DICT_VAL(newFrozenDict(&map)));
    freeMap(&map);
  }

  {
    Map map;
    initMap(&map);
    mapSetN(&map, "LEFTX", NUMBER_VAL(0));
    mapSetN(&map, "LEFTY", NUMBER_VAL(1));
    mapSetN(&map, "RIGHTX", NUMBER_VAL(2));
    mapSetN(&map, "RIGHTY", NUMBER_VAL(3));
    mapSetN(&map, "TRIGGERLEFT", NUMBER_VAL(4));
    mapSetN(&map, "TRIGGERRIGHT", NUMBER_VAL(5));
    mapSetN(&module->fields, "CONTROLLER_AXIS", FROZEN_DICT_VAL(newFrozenDict(&map)));
    freeMap(&map);
  }

  {
    Map map;
    initMap(&map);
    mapSetN(&map, "LEFT", NUMBER_VAL(BUTTON_LEFT));
    mapSetN(&map, "RIGHT", NUMBER_VAL(BUTTON_RIGHT));
    mapSetN(&map, "UP", NUMBER_VAL(BUTTON_UP));
    mapSetN(&map, "DOWN", NUMBER_VAL(BUTTON_DOWN));
    mapSetN(&map, "O", NUMBER_VAL(BUTTON_O));
    mapSetN(&map, "X", NUMBER_VAL(BUTTON_X));
    mapSetN(&map, "START", NUMBER_VAL(BUTTON_START));
    mapSetN(&module->fields, "BUTTON", FROZEN_DICT_VAL(newFrozenDict(&map)));
    freeMap(&map);
  }

  {
    Map map;
    ColorEntry *entry;
    Value *colors;
    size_t colorCount = 0, i;
    initMap(&map);
    for (entry = colorEntries; entry->name; entry++) {
      mapSetN(&map, entry->name, COLOR_VAL(entry->color));
      colorCount++;
    }
    mapSetN(&module->fields, "COLOR", FROZEN_DICT_VAL(newFrozenDict(&map)));
    freeMap(&map);
    colors = malloc(sizeof(Value) * colorCount);
    for ((void)(entry = colorEntries), i = 0; entry->name; entry++, i++) {
      colors[i] = COLOR_VAL(entry->color);
    }
    mapSetN(&module->fields, "COLORS", FROZEN_LIST_VAL(copyFrozenList(colors, colorCount)));
    free(colors);
  }

  mapSetN(&module->fields, "PRESSED",   NUMBER_VAL(QUERY_PRESSED));
  mapSetN(&module->fields, "RELEASED",  NUMBER_VAL(QUERY_RELEASED));
  mapSetN(&module->fields, "HELD",      NUMBER_VAL(QUERY_HELD));

  LOCAL_GC_UNPAUSE(gcPause);

  if (!initSDL()) {
    return UFALSE;
  }

  {
    int numkeys;
    keyboardState = SDL_GetKeyboardState(&numkeys);
    /* numkeys is expected to be 512 */
    if (numkeys < ((int)SCANCODE_KEY_COUNT)) {
      panic(
        "numKeys(%d) < SCANCODE_KEY_COUNT(%d)",
        (int)numkeys,
        (int)SCANCODE_KEY_COUNT);
    }
  }

  mixerConfigMutex = SDL_CreateMutex();
  if (!mixerConfigMutex) {
    return sdlError("SDL_CreateMutex");
  }

  playbackDataMutex = SDL_CreateMutex();
  if (!playbackDataMutex) {
    return sdlError("SDL_CreateMutex");
  }

  return UTRUE;
}

static CFunction func = { impl, "gg", 1 };

void addNativeModuleGG(void) {
  addNativeModule(&func);
}
