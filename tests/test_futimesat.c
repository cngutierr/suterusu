// EXAMPLE CODE BELOW TAKE FROM 
// https://www.ibm.com/support/knowledgecenter/ssw_i5_54/apis/utime.htm
#include <utime.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

main() {
        int file_descriptor;
        char fn[]="/tmp/utime.file";
        struct stat info;
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
        if ((file_descriptor = creat(fn, S_IWUSR)) < 0)
                perror("creat() error");
        else {
                puts("before futimets()");
                stat(fn,&info);
                printf("  utime.file modification time is %ld\n",
                                info.st_mtime);
                
                if (futimesat(AT_FDCWD, fn, &ts) != 0)
                        perror("futimens() error");
                else {
                        puts("after futimens()");
                        stat(fn,&info);
                        printf("  utime.file modification time is %ld\n",
                                        info.st_mtime);
                }
                close(file_descriptor);
                //unlink(fn);
        }
}
