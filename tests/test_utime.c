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
        char fn[]="utime.file";
        struct utimbuf ubuf;
        struct stat info;

        if ((file_descriptor = creat(fn, S_IWUSR)) < 0)
                perror("creat() error");
        else {
                close(file_descriptor);
                puts("before utime()");
                stat(fn,&info);
                printf("  utime.file modification time is %ld\n",
                                info.st_mtime);
                ubuf.modtime = 0;       /* set modification time to Epoch */
                time(&ubuf.actime);
                if (utime(fn, &ubuf) != 0)
                        perror("utime() error");
                else {
                        puts("after utime()");
                        stat(fn,&info);
                        printf("  utime.file modification time is %ld\n",
                                        info.st_mtime);
                }
                unlink(fn);
        }
}
