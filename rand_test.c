#include "rand_test.h"

int ratio[16] = {-4, -2, -2,  0, 
                 -2,  0,  0,  2, 
                 -2,  0,  0,  2,
                  0,  2,  2,  4};
/* calulate the ratio of 1 and 0 bits. If there are
 * an even amount of bits, return 0. Positive means
 * that there are more ones than zeros. Negative means
 * that there are more zeros than ones. */
int bit_sum(unsigned char* buf, unsigned int buf_len)
{
    unsigned int sum = 0;
    unsigned int i;

    for(i = 0; i < buf_len; i++)
        sum += ratio[buf[i] & 0x0f] + ratio[buf[i] >> 4];

    return sum;
}

/*count the number of 1 bits in a single byte*/
unsigned int bit_count(unsigned char byte)
{   
    const unsigned int lookup[16] = {0, 1, 1, 2, 1, 2, 2, 3,
                                     1, 2, 2, 3, 2, 3, 3, 4};
    return lookup[0x0f & byte] + lookup[byte >> 4];
}

/*counts the numbers of 1 bits in a buffer*/
unsigned int ones_count(unsigned char* buf, unsigned int buf_len)
{   
    unsigned int sum = 0;
    unsigned int i;
    for(i = 0; i < buf_len; i++)
        sum += bit_count(buf[i]);
    return sum;
}

/*count the number of times a run of bits switches from
 * a zero to a one or vice versa */
unsigned int run_count(unsigned char* buf, unsigned int buf_len)
{
    unsigned int run_sum = 0;
    unsigned int previous;
    int check_boundry = 0;
    unsigned int i, j;
    for(i = 0; i < buf_len; i++)
    {
        if(check_boundry > 0)
            run_sum += (buf[i] & 0x01) ^ previous;
        for(j = 1; j < BYTE_SIZE; j++)
            run_sum += ((buf[i] >> j) & 0x01) ^ ((buf[i] >> (j-1)) & 0x01);
         previous = (buf[i] >> (BYTE_SIZE - 1)) & 0x01;
         check_boundry = 1;
    }
    return run_sum + 1;
}
   

unsigned int freq_monobit_test(unsigned char *buf, int len)
{
    float s_obs;
    int sum;
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
    /*
     * Next, calculate the the stats test
     * which is:
     *
     * abs(S_n)/sqrt(n)
     *
     * s_obs as defined by NIST 
     */
    
    len *= BYTE_SIZE;
    tmp.i = no_math_sqrt(len);
    s_obs = (sum < 0 ? -1*sum : sum ) / tmp.f;
    
    
    /*
     * Finally, compute the P-value:
     * erfc(s_obs}/sqrt(2))
     * 
     * P-value as defined by NIST
     */
    tmp.i = no_math_erfc(s_obs/SQRT2);
    return tmp.i;
}

void D4_B4096_test(D4_B4096* D4B4096_results, unsigned char *buf, unsigned int buf_size, unsigned int block_size)
{
    int block_sum;
    float sum = 0.0;
    unsigned int num_blocks = buf_size / block_size;
    unsigned int i;
    float pi, chi_sq;
    union Number tmp, tmp2;
    float s_obs;
    int monobit_sum = 0;
    //pre-test
    float pi_runs = 0.0;
    unsigned int run_sum = 0;  
       
    for(i = 0; i < num_blocks; i++)
    {
        //monobit stuff
        monobit_sum += bit_sum(&buf[i*block_size], block_size);
        //freq block stuff
        block_sum = ones_count(&buf[i*block_size], block_size);
        pi = ((float) block_sum / ((float)block_size * BYTE_SIZE)) - 0.5;
        sum += pi*pi;
        //runs stuff
        pi_runs += (float) ones_count(&buf[i*block_size], block_size);
    }
    //monobit stuff
    s_obs = (monobit_sum < 0 ? -1 * monobit_sum : monobit_sum ) / SQRT4096;
    tmp.i = no_math_erfc(s_obs/SQRT2);
    D4B4096_results->monobit_freq = tmp.f;

    //freq block stuff
    chi_sq = 4.0 * (float) block_size * BYTE_SIZE * sum;
    tmp.i = no_math_igamc(buf_size, chi_sq/2.0);
    D4B4096_results->block_freq = tmp.f;
    
    //runs stuff
    pi_runs = pi_runs / ((float) buf_size * BYTE_SIZE);
 
    if( ((pi_runs - 0.5) < 0 ? -1.0 * (pi_runs -0.5): pi_runs) > 0.011048)
        D4B4096_results->runs =  0.0;
    else
    {
        run_sum = run_count(buf, buf_size);
        DEBUG_RAND_TEST("Made it pass the first test\n");
        DEBUG_RAND_TEST("run sum: %lu\n", run_sum);
        tmp.f = pi_runs;
        DEBUG_RAND_TEST("pi run: %x\n", tmp.i);
        tmp.f = run_sum - 2.0 * buf_size * BYTE_SIZE * pi_runs *(1.0 - pi_runs);
        DEBUG_RAND_TEST("tmp1: %x\n", tmp.i);
        tmp.f = tmp.f < 0.0 ? tmp.f * -1.0: tmp.f;
        DEBUG_RAND_TEST("tmp2: %x\n", tmp.i);
        tmp2.f = (2.0 *pi_runs * (1.0 - pi_runs) * 256.0);
        DEBUG_RAND_TEST("tmp3: %x\n", tmp2.i);
        tmp.i = no_math_erfc(tmp.f / (2.0*pi_runs*(1.0-pi_runs)*256.0));
        D4B4096_results->runs  = tmp.f;
    }  
}

void D2_B4096_test(D2_B4096* D2B4096_results, unsigned char *buf, unsigned int buf_size, unsigned int block_size)
{
    int block_sum;
    float sum = 0.0;
    unsigned int num_blocks = buf_size / block_size;
    unsigned int i;
    float pi, chi_sq;
    union Number tmp;
    float s_obs;
    int monobit_sum = 0;
    
    for(i = 0; i < num_blocks; i++)
    {
        //monobit stuff
        monobit_sum += bit_sum(&buf[i*block_size], block_size);
        //freq block stuff
        block_sum = ones_count(&buf[i*block_size], block_size);
        pi = ((float) block_sum / ((float)block_size * BYTE_SIZE)) - 0.5;
        sum += pi*pi;
    }
    //monobit stuff
    s_obs = (monobit_sum < 0 ? -1 * monobit_sum : monobit_sum ) / SQRT4096;
    tmp.i = no_math_erfc(s_obs/SQRT2);
    D2B4096_results->monobit_freq = tmp.f;

    //freq block stuff
    chi_sq = 4.0 * (float) block_size * BYTE_SIZE * sum;
    tmp.i = no_math_igamc(buf_size, chi_sq/2.0);
    D2B4096_results->block_freq = tmp.f;
}

unsigned int freq_block(unsigned char* buf, unsigned int buf_size, unsigned int block_size)
{
    int block_sum;
    float sum = 0.0;
    unsigned int num_blocks = buf_size / block_size;
    unsigned int i;
    float pi, chi_sq;
    for(i = 0; i < num_blocks; i++)
    {
        block_sum = ones_count(&buf[i*block_size], block_size);
        pi = ((float) block_sum / ((float)block_size * BYTE_SIZE)) - 0.5;
        sum += pi*pi;
    }
    chi_sq = 4.0 * (float) block_size * BYTE_SIZE * sum;
    return no_math_igamc(buf_size, chi_sq/2.0);
}


int common_template_test(unsigned char* buf, int len)
{
    int i;
    int ones_check = 1;
    DEBUG_RAND_TEST("template test\n");
    for(i = 0; i < len; i++)
    {
        //check if the buffer is all ones
        if((0xFF & buf[i]) != 0xFF)
        {
            ones_check = 0;
            break;
        }
    }
    if(ones_check)
    {
        DEBUG_RAND_TEST("Ones template detected!\n");
        return 1;
    }
    for(i = 0; i < len; i++)
    {
        //check if the buffer is all zeros
        if((0xFF & !buf[i]) != 0xFF)
            return 0;    
    }
    DEBUG_RAND_TEST("Zero template detected!\n");
    return 1;
}

int run_rand_check(unsigned char* buf, int len, const int max_depth)
{
    int ret = 0;
    union Number tmp;
    D2_B4096 D2B4096_results;
    D4_B4096 D4B4096_results;
    
    if(max_depth == 1 && len == 4096)
    {
        tmp.i = freq_block(buf, len, 41);
        DEBUG_RAND_TEST("freq_block=%x\n", tmp.i);
        return tmp.f <= 0.0023 ? 0 : 1;
    }
    else if(max_depth == 2 && len == 4096)
    {   
        D2_B4096_test(&D2B4096_results, buf, len, 41);
        DEBUG_RAND_TEST("monobit=%x\n", &D2B4096_results.monobit_freq);
        DEBUG_RAND_TEST("freqblock=%x\n", &D2B4096_results.block_freq);
        
        //classification portion
       if(D2B4096_results.block_freq <= 0.0023)
       {
         if(D2B4096_results.monobit_freq <= 0.9603)
            return 0;
         else
            return 1;
       }
       else
       {
         if(D2B4096_results.monobit_freq <= 0.0001)
            return 0;
         else
            return 1;
       }
    }
    else if(max_depth == 1 && len == 16)
    {
        tmp.i = freq_monobit_test(buf, len);
        DEBUG_RAND_TEST("monobit_freq=%x\n", tmp.i);
        return tmp.f <= 0.1866 ? 0 : 1;
    }
    else if(max_depth == 4 && len == 4096)
    {
        D4_B4096_test(&D4B4096_results, buf, len, 41);
        tmp.f = D4B4096_results.monobit_freq;
        DEBUG_RAND_TEST("monobit_freq=%x\n", tmp.i);
        tmp.f = D4B4096_results.block_freq;
        DEBUG_RAND_TEST("block_freq=%x\n", tmp.i);
        tmp.f = D4B4096_results.runs;
        DEBUG_RAND_TEST("runs=%x\n", tmp.i);
        if(D4B4096_results.block_freq <= 0.0023)
        {
            //left branch
            if(D4B4096_results.monobit_freq <= 0.9603)
            {
                if(D4B4096_results.runs <= 0.5144)
                    return 0;
                else
                {
                    if(D4B4096_results.block_freq <= 0.0002)
                        return 0;
                    else
                        return 1;
                }
            }
            else
                return 1;
        }
        else
        {
            if(D4B4096_results.runs <= 0.0009)
                return 0;
            else
            {
                if(D4B4096_results.block_freq <= 0.0054)
                {
                    if(D4B4096_results.block_freq <= 0.0043)
                        return 1;
                    else
                        return 0;
                } 
                else
                {   
                    /*
                    if(D4B4096_results.block_freq <= 0.0828)
                        return 1;
                    else
                        return 1;  */
                    return 1;
                }
            }
        }
    }
    else
    {
        DEBUG_RAND_TEST("MAX_DEPTH and BUF_SIZE combo not defined!!\n");
        return 0;
    }
    return ret;
}
