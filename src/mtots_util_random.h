#ifndef mtots_util_random_h
#define mtots_util_random_h

#include "mtots_util_string.h"

/* Mersenne Twister (MT19937) */
typedef struct Random {
  u32 state[624];
  size_t index;
} Random;

void initRandom(Random *random, u32 seed);
u32 randomNext(Random *random);
double randomFloat(Random *random);
u32 randomInt(Random *random, u32 n);

#endif /*mtots_util_random_h*/
