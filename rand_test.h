#ifndef RANDTEST_H
#define RANDTEST_H
#include "no_math.h"
#include "common.h"
#define BYTE_SIZE 8
typedef struct BlockConsts
{
    unsigned int size;
    unsigned int K;
    unsigned int N;
    unsigned int V[6];
    float pi[6];
} BlockConsts;

static const BlockConsts small_block_consts =  {4, 3, 16, {1, 2, 3, 4},
                                            {0.2148, 0.3672, 0.2305, 0.1875}};
static const BlockConsts medium_block_consts = {6, 5, 49, {4, 5, 6, 7, 8, 9},
                                            {0.1174, 0.2430, 0.2493, 0.1752, 0.1027, 0.1124}};
static const BlockConsts large_block_consts =  {6, 5, 49, {6, 7, 8, 9, 10, 11},
                                            {0.1170, 0.2460, 0.2523, 0.1755, 0.1027, 0.1124}};
extern int ratio[];
/*
 *Helper structs for classification tree
 */

//depth = 2, buffer = 4096
typedef struct D2_B4096
{
    float block_freq;
    float monobit_freq;
}D2_B4096;

typedef struct D4_B4096
{
    float block_freq;
    float monobit_freq;
    float runs;
}D4_B4096;

typedef struct D3_B4096
{
    float block_freq;
    float monobit_freq;
    float runs;
    float one_runs;
}D3_B4096;
/*
 * Helper functions
 */

/* bit_sum
 * negative value implies there are more
 * zeros than ones. Positive means there
 * are more ones than zeros. Zero means
 * equal ones and zeros
 */
int bit_sum(unsigned char *buf, unsigned int len);

/*count the number of 1 bits in a single byte*/
unsigned int bit_count(unsigned char byte);

/*counts the numbers of 1 bits in a buffer*/
unsigned int ones_count(unsigned char* buf, unsigned int buf_len);

/*count the number of times a run of bits switches from
 *  * a zero to a one or vice versa */
unsigned int run_count(unsigned char* buf, unsigned int buf_len);

/*
 * take in a buffer, check if random, 
 * return 1 if random, 0 if not random
 */
unsigned int freq_monobit_test(unsigned char *buf, int len);

unsigned int freq_block(unsigned char* buf, unsigned int buf_size, unsigned int block_size);
/*
 * Check for common templates such as 0xFF, etc.
 * See Peter Gutmann's paper
 */
int common_template_test(unsigned char* buf, int len);

/*
 * Run the randomness checker. Checks the config
 * and calculates all the randomness checks
 */
int run_rand_check(unsigned char* buf, int len, const int max_depth);

/*
 * Randomoness test with classification tree of depth 2 for array size of 4096
 */
void D2_B4096_test(D2_B4096* results, unsigned char *buf,
                   unsigned int buf_size, unsigned int block_size);

void D4_B4096_test(D4_B4096* results, unsigned char *buf,
                   unsigned int buf_size, unsigned int block_size);

void D3_B4096_test(D3_B4096* D3B4096_results, unsigned char *buf, 
                   unsigned int buf_size, unsigned int block_size);

unsigned int longest_one_run(unsigned char* buf, unsigned int buf_size);
unsigned int get_v_bucket(unsigned int ones_run, const BlockConsts* block_consts);

#endif
