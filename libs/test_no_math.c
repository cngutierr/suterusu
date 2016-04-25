#include <stdio.h>
#include <math.h>
#include "no_math.h"
int main()
{
        float no_math_tmp;
        float math_tmp;
        float i;
        int j;
        /* test no_math_erfc */
        printf("**test no_math_erfc**\n");
        for(i = -1.0; i <= 2.0; i += 0.25)
        {
            no_math_tmp = no_math_erfc(i);
            math_tmp = erfc(i);
            printf("no_math_erfc(%f) = %f\n", i, no_math_tmp);
            printf("math_erfc(%f)    = %f\n", i, math_tmp);
            printf("delta = %f\n\n", fabs(no_math_tmp - math_tmp));
        }
        printf("**test no_math_sqrt**\n");
        /* test my_sqrt */
        for(j=0; j < 10; j++)
        {
            no_math_tmp = no_math_sqrt(j);
            math_tmp = sqrt(j);
            printf("no_math_sqrt(%i) = %f\n", j, no_math_tmp);
            printf("math_sqrt(%i) = %f\n", j, math_tmp);
            printf("delta = %f\n\n", fabs(no_math_tmp - math_tmp));
        }
    return 0;
}
