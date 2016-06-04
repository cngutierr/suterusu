#include <stdio.h>
#include <stdlib.h>
#include <time.h>
/*
 * Usage:
 * ./latencyTest 100 4096 4096
 * Argument 1: the number of samples to generate
 * Argument 2: the size, in bytes of the file to be writen
 * Arugment 3: the size of the fwrite buffer
 */

struct timespec diff(struct timespec start, struct timespec end);

int main(int argc, char* argv[])
{
    if(argc < 5)
        printf("Not enough arguments. Should be SAMPLES, FILE_SIZE, BUFFER_SIZE, <0|1>\n");
    else
    {
        /* args passed in for generating latency*/
        int samples = atoi(argv[1]);
        int filesize = atoi(argv[2]);
        int buffer_size = atoi(argv[3]);
        int type_of_timer = atoi(argv[4]) == 0 ? CLOCK_PROCESS_CPUTIME_ID : CLOCK_REALTIME;
        /* Garbage file we will write to */
        FILE* out_file;
        out_file = fopen("/tmp/junk.txt", "w");
        /* to file the write buffer with random data */
        FILE* rand_file;
        rand_file = fopen("/dev/urandom","r"); 
        char* write_buf = (char *) malloc(buffer_size);
        /* keep track on how much data is left before we finish writing
         * the file */
        int data_left = filesize;
        
        /* times yo */
        struct timespec start_t, end_t, diff_t;
        printf("filesize=%i, buffer_size=%i, clock=%s\n", filesize, buffer_size,
                    type_of_timer == CLOCK_REALTIME ? "CLOCK_REAL" : "CLOCK_PROCESS_CPUTIME_ID");
        for(int i = 0; i < samples; i++)
        {
             /* create some random data */
            fread(write_buf, sizeof(char), buffer_size, rand_file);
            clock_gettime(type_of_timer, &start_t);
            while(data_left >= buffer_size)
            {
                /*
                 * time it!
                 */
                fwrite(write_buf, sizeof(char), buffer_size, out_file);
                data_left -= buffer_size;
            }
            clock_gettime(type_of_timer, &end_t);
            diff_t = diff(start_t, end_t);
            printf("%0.4f\n", diff_t.tv_sec/1000.0 + (diff_t.tv_nsec/100000.0));
            /* we have some leftover data that we need to save out */
            if(data_left != 0)
            {
                /* write that shit out */   

                printf("Too lazy\n");
            }
            /* print the latencies out */
            
            fclose(out_file);
            out_file = fopen("/tmp/junk.txt", "w");
            data_left = filesize;
        }
        
        free(write_buf);
        fclose(rand_file);
        fclose(out_file);
    }
}

/* Based on the code found at guyrutenberg.com */
struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec ret;
        
    if(end.tv_nsec - start.tv_nsec < 0)
    {
        ret.tv_sec = end.tv_sec - start.tv_sec - 1;
        ret.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
    }
    else
    {
        ret.tv_sec = end.tv_sec - start.tv_sec;
        ret.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return ret;
}
