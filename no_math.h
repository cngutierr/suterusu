#ifndef NOMATH_H
#define NOMATH_H
#include "common.h"

#define SQRT2       1.4142
//bits are converted to bits before calc
#define SQRT16      11.3137
#define SQRT512     64
#define SQRT4096    181.0193

#define DSQRT16    0.17677
#define DSQRT512   0.03125
#define DSQRT4096  0.01104
#define SQRT2_16   32
#define SQRT2_512  181.0193
#define SQRT2_4096 512
#define _2_16        256
#define _2_512       8192
#define _2_4096      65536

/*
 * The union below is a shortcut to 
 * convert floats and ints. Without it,
 * we will get some complaints about sse
 * registers
 */
union Number 
{
    float f;
    unsigned int i;
};

/*
 * no_math generated constant values
 */
#define ERFC_TAB_MIN -2.5
#define ERFC_TAB_MAX 2.5
#define ERFC_ARRAY_MAX 500
extern float ERFC_TAB[];

#define SQRT_TAB_MIN 0
#define SQRT_TAB_MAX 4096
extern float SQRT_TAB[];

#define IGAMC_16_STEP  0.01
extern float IGAMC_16[];

#define IGAMC_512_STEP  0.125
extern float IGAMC_512[];

#define IGAMC_4096_STEP  0.125
extern float IGAMC_4096[];
/*
 * Func prototypes for no_math lib
 */
unsigned int no_math_erfc(float in);
unsigned int no_math_sqrt(int in);
unsigned int no_math_igamc(int buf_size, float value);

#endif
