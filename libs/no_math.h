#ifndef NOMATH_H
#define NOMATH_H

/*
 * no_math generated constant values
 */
#define ERFC_TAB_MIN -2.500000
#define ERFC_TAB_MAX 2.500000
extern float ERFC_TAB[];

#define SQRT_TAB_MIN 0.000000
#define SQRT_TAB_MAX 4096.000000
extern float SQRT_TAB[];

/*
 * Func prototypes for no_math lib
 */
float no_math_erfc(float in);
float no_math_sqrt(int in);
#endif
