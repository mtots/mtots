#ifndef mtots_vm_h
#define mtots_vm_h

#include "mtots.h"
#include "mtots_class_range.h"
#include "mtots_class_sb.h"
#include "mtots_m_bmon.h"
#include "mtots_m_sys.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * U8_COUNT)
#define MAX_ERROR_STRING_LENGTH 2048
#define SIGNAL_HANDLERS_COUNT 32

#define LOCAL_GC_PAUSE(flag) \
  do {                       \
    flag = vm.localGCPause;  \
    vm.localGCPause = UTRUE; \
  } while (0)

#define LOCAL_GC_UNPAUSE(flag) \
  do {                         \
    vm.localGCPause = flag;    \
  } while (0)

typedef struct CallFrame {
  ObjClosure *closure;
  u8 *ip;
  Value *slots;
} CallFrame;

typedef struct VM {
  CallFrame frames[FRAMES_MAX];
  i16 frameCount;
  Value stack[STACK_MAX];
  Value *stackTop;
  Map globals;
  Map modules;            /* all preloaded modules */
  Map nativeModuleThunks; /* Map of CFunctions */
  Map frozenLists;        /* table of all interned frozenLists */
  Map frozenDicts;        /* a table of all interned FrozenDicts */

  /* If set, will be run once the main script finishes -
   * a hack to get gg to work without an explicit
   * call to `window.mainLoop()` */
  CFunction *runOnFinish;

  String *emptyString;
  String *initString;
  String *iterString;
  String *lenString;
  String *reprString;
  String *addString;
  String *subString;
  String *mulString;
  String *divString;
  String *floordivString;
  String *modString;
  String *powString;
  String *negString;
  String *containsString;
  String *nilString;
  String *trueString;
  String *falseString;
  String *getitemString;
  String *setitemString;
  String *sliceString;
  String *getattrString;
  String *setattrString;
  String *callString;
  String *redString;
  String *greenString;
  String *blueString;
  String *alphaString;
  String *rString;
  String *gString;
  String *bString;
  String *aString;
  String *wString;
  String *hString;
  String *xString;
  String *yString;
  String *zString;
  String *typeString;
  String *widthString;
  String *heightString;
  String *minXString;
  String *minYString;
  String *maxXString;
  String *maxYString;

  ObjClass *sentinelClass;

  ObjClass *nilClass;
  ObjClass *boolClass;
  ObjClass *numberClass;
  ObjClass *stringClass;
  ObjClass *rangeClass;
  ObjClass *rangeIteratorClass;
  ObjClass *vectorClass;
  ObjClass *pointerClass;
  ObjClass *bufferClass;
  ObjClass *listClass;
  ObjClass *frozenListClass;
  ObjClass *dictClass;
  ObjClass *frozenDictClass;
  ObjClass *functionClass;
  ObjClass *classClass;

  ObjUpvalue *openUpvalues;

  Memory memory;
  ubool enableGCLogs;
  ubool enableMallocFreeLogs;
  ubool enableLogOnGC;
  ubool localGCPause;
  ubool trap;

  int signal;
  Value signalHandlers[SIGNAL_HANDLERS_COUNT];

  ObjList *atExitCallbacks;
} VM;

extern VM vm;

void initVM(void);
void freeVM(void);
Status interpret(const char *source, ObjModule *module);
void defineGlobal(const char *name, Value value);
void closeUpvalues(Value *last);

Status checkAndHandleSignals(void);

#endif /*mtots_vm_h*/
