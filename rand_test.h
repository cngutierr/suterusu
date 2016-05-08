#ifndef RANDTEST_H
#define RANDTEST_H
#include "no_math.h"
#include "common.h"

/*
 * Helper functions
 */

/* bit_sum
 * negative value implies there are more
 * zeros than ones. Positive means there
 * are more ones than zeros. Zero means
 * equal ones and zeros
 */
int bit_sum(unsigned char *buf, int len);

/*
 * take in a buffer, check if random, 
 * return 1 if random, 0 if not random
 */
int freq_monobit_test(unsigned char *buf, int len);

/*
 * Check for common templates such as 0xFF, etc.
 * See Peter Gutmann's paper
 */
int common_template_test(unsigned char* buf, int len);
#endif
