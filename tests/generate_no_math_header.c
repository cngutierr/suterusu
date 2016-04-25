#include <stdio.h>
#include <string.h>
#include <math.h>
/*
 * generate the erfc table
 */
void print_func(float low, float upper, float step, char *name, double (*func)(double))
{
    float index = low;
    int count = 0;
    
    printf("#define %s_TAB_MIN %f\n", name, low);
    printf("#define %s_TAB_MAX %f\n", name, upper);
    printf("float %s_TAB[%0.0f] = \n{", name, (upper - low)/step);
    while(index < upper)
    {
        
        printf("%f, ", func(index));
        /* do some rounding magic to make sure we are stepping correctly  */
        index = roundf(100*(index + step)) / 100.0;
        count++;
        if(count % 10 == 0)
            printf("\n");
    }
    
    printf("};\n\n");
}


int main(int argc, char *argv[])
{
    if(argc > 1)
    {
    if(strcmp(argv[1], "all") == 0)
    {
        print_func(-2.5, 2.5, 0.01, "ERFC", erfc);
        print_func(0.0, 4096, 1.0, "SQRT", sqrt);
    }
    else if(strcmp(argv[1], "sqrt") == 0)
    {
        print_func(0.0, 4096, 1.0, "SQRT", sqrt);
    }
    else if(strcmp(argv[1], "erfc") == 0)
    {
        print_func(0.0, 4096, 1.0, "SQRT", erfc);
    }
    }
    /* default by printing all*/
    else
    {
        print_func(-2.5, 2.5, 0.01, "ERFC", erfc);
        print_func(0.0, 4096, 1.0, "SQRT", sqrt);
    }
    return 0;
    
}
