#include <stdio.h>
#include "rand_test.h"

#define PRINT_SUM 1
void check_sum(unsigned char *buf, int buflen, int expected, int flag)
{
  int i;
  int sum = bit_sum(buf, buflen);
  if(sum != expected)
    printf("ERROR: Expected %i but got %i\n", expected, sum);
  
  if(flag & PRINT_SUM)
  {
    printf("bit_sum(0x");
    for(i = 0; i < buflen; i++)
        printf("%02x", buf[i]);
    printf(") = %i\n", bit_sum(buf, 2));
  }
}
void check_randomness(unsigned char *buf, int buflen)
{
    int i;
    printf("freq_monobit(");
    for(i = 0; i < buflen; i++)
        printf("%02x", buf[i]);
    printf("): %s\n\n", freq_monobit_test(buf, buflen) == 1 ? "yes": "no");
}
int main()
{
    unsigned char all_one[2] = {0xFF, 0xFF};
    check_sum(all_one, 2, 16, PRINT_SUM);
    check_randomness(all_one, 2);

    unsigned char all_zero[2] = {0x00, 0x00};
    check_sum(all_zero, 2, -16, PRINT_SUM);
    check_randomness(all_zero, 2);
    
    unsigned char equal[2] = {0xAA, 0xAA};
    check_sum(equal, 2, 0, PRINT_SUM);
    check_randomness(equal, 2);
    
    unsigned char eight[2] = {0xFA, 0xAF};
    check_sum(eight, 2, 8, PRINT_SUM);
    check_randomness(eight, 2);
    

    return 0;
}
