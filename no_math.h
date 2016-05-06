#ifndef NOMATH_H
#define NOMATH_H
#include "common.h"

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

/*
 * Func prototypes for no_math lib
 */
unsigned int no_math_erfc(float in);
unsigned int no_math_sqrt(int in);
#endif
