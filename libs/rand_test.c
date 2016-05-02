#include "rand_test.h"
#include <stdio.h>
int bit_sum(unsigned char *buf, int len)
{
  int i;
  int j;
  int sum = 0;
  for(i = 0; i < len; i++)
  {
    for(j = 0; j < 8; j++)
    {
        sum += 2 * ((buf[i] >> j) & 1) - 1;
    }
  }
 return sum; 
}

int freq_monobit_test(unsigned char *buf, int len)
{
    float s_obs;
    int sum;
    float p_value;
    union Number tmp;
    /*
     * first, calculate the bits sum, 
     * if bit === 0: 1, else -1. A bit
     * bit sum of zero means that there
     * are equal numbers of 1 and 0 bits
     * in the buffer.
     *
     * S_n as defined by NIST
     */
    sum = bit_sum(buf, len);
    printf("\nsum: %i\n", sum);
    /*
     * Next, calculate the the stats test
     * which is:
     *
     * abs(S_n)/sqrt(n)
     *
     * s_obs as defined by NIST 
     */
    
    len *= 8;
    tmp.i = no_math_sqrt(len);
    s_obs = (sum < 0 ? -1*sum : sum ) / tmp.f;
    printf("\nlen: %i  nsqrt: %f\n", len, tmp.f);
    printf("\ns_obs: %f\n", s_obs);
    
    /*
     * Finally, compute the P-value:
     * erfc(s_obs}/sqrt(2))
     * 
     * P-value as defined by NIST
     */
    tmp.i = no_math_sqrt(2);
    tmp.i = no_math_erfc(s_obs/tmp.f);
    printf("\np_val: %f\n", tmp.f);
    p_value = tmp.f;
    return p_value < 0.01 ? 0 : 1;
}
