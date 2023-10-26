#ifndef mtots_h
#define mtots_h

#include "mtots_import.h"
#include "mtots_macros.h"
#include "mtots_ops.h"

/* Functions for calling mtots functions and methods from C
 * These functions are implemented in mtots_vm.c */

/* Given that a function and its arguments are on the stack,
 * call the given function.
 *
 * Before calling this function, the stack must look like:
 *   TOP
 *     lastArg
 *     ...
 *     firstArg
 *     functionValue
 *   BOT
 *
 * Once the function is called, all arguments and the function
 * itself will be popped from the stack and replaced by the single
 * return value.
 *
 */
Status callFunction(i16 argCount);

/* Given that a value and its method's arguments are on the stack,
 * call a method on the given value.
 *
 * Before calling this function, the stack must look like:
 *   TOP
 *     lastArg
 *     ...
 *     firstArg
 *     receiverValue
 *   BOT
 *
 * Once the method is called, all arguments and the receiver
 * itself will be popped from the stack and replaced by the single
 * return value.
 *
 */
Status callMethod(String *methodName, i16 argCount);

/*
 * Native module bodies should be a CFunction that accepts
 * Exactly one argument, the module
 */
void addNativeModule(CFunction *func);

void registerMtotsAtExitCallback(Value callback);

/* Stack manipulation */
void push(Value value);
Value pop(void);

void locallyPauseGC(ubool *flag);
void locallyUnpauseGC(ubool flag);

#endif /*mtots_h*/
