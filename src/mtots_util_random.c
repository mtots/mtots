#include "mtots_util_random.h"

/* References:
 * https://github.com/notr1ch/opentdm/blob/master/mt19937.c
 * http://facweb.cs.depaul.edu/sjost/csc433/documents/mersenne-twister-pseudocode.txt
 */

#define N 624
#define M 397
#define MATRIX_A 0x9908b0df
#define UPPER_MASK 0x80000000
#define LOWER_MASK 0x7fffffff

void initRandom(Random *random, u32 seed) {
  size_t i = 0;
  u32 *mt = random->state;
  mt[0] = seed;
  for (i = 1; i < N; i++) {
    mt[i] = 1812433253UL * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i;
  }
  random->index = N;
}

static size_t modIndex(size_t i, size_t size) {
  return i < size ? i : i - size;
}

static void randomNextState(Random *random) {
  u32 *mt = random->state;
  size_t i;

  for (i = 0; i < N; i++) {
    u32 a = (mt[i] & UPPER_MASK) | (mt[modIndex(i + 1, N)] & LOWER_MASK);
    u32 b = mt[modIndex(i + M, N)] ^ (a >> 1);
    mt[i] = a & 1 ? b ^ MATRIX_A : b;
  }

  random->index = 0;
}

u32 randomNext(Random *random) {
  u32 *mt = random->state;
  u32 y;

  if (random->index >= N) {
    randomNextState(random);
  }

  y = mt[random->index++];
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680;
  y ^= (y << 15) & 0xefc60000;
  y ^= (y >> 18);

  return y;
}

/* Returns a floating point number X such that 0.0 <= X < 1.0 */
double randomFloat(Random *random) {
  u32 i;
  do {
    i = randomNext(random);
  } while (i == U32_MAX);
  return i / (double)U32_MAX;
}

/* Return an integer uniformly randomly selected from the range [0, n]
 * (the range is inclusive of both 0 and n) */
u32 randomInt(Random *random, u32 n) {
  u32 rem, count, limit, value;
  if (n == U32_MAX) { /* special case where n+1 would overflow */
    return randomNext(random);
  }
  count = n + 1;
  rem = U32_MAX % count;
  if (rem == n) { /* i.e. U32_COUNT % count == 0 */
    return randomNext(random) % count;
  }
  limit = U32_MAX - rem; /* i.e. U32_COUNT - (U32_COUNT % count) */
  do {
    value = randomNext(random);
  } while (value >= limit);
  return value % count;
}
