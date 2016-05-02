#include "common.h"
#include "logging.h"
#include <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/ktime.h>
#include <linux/utime.h>
//Needed to use readlink
//logger stuff

#define DEFAULT_FILEPATH_SIZE 256
#define UTIME_CALL 1
#define FUTIMESAT_CALL 2
#define UTIMENSAT_CALL 3
#define ALLOWED_TIME_DIFFERENCE 3

// pointers to the system call functions being hijacked
asmlinkage long (*sys_utime)(char __user *filename, struct utimbuf __user *times);
asmlinkage long (*sys_utimes)(char __user *filename, struct timeval  __user *times);
asmlinkage long (*sys_futimesat)(int dfd, char __user *filename, struct timeval __user *utimes);
asmlinkage long (*sys_utimensat)(int dfd, char __user *filename, struct timespec __user *utimes, int flags);

//pointers to system calls used by this program
asmlinkage long (*sys_readlink)(const char __user *path, char __user *buf, int bufsiz);
asmlinkage long (*sys_getcwd)(char __user *buf, unsigned long size);

void general_timestamp_processor(int dfd, char __user *filename,
                                 struct utimbuf __user *times,
                                 struct timeval __user *vutimes,
                                 struct timespec __user *utimes,
                                 int flags);


asmlinkage long n_sys_utime (char __user *filename, struct utimbuf __user *times)
{

    long ret;
    DEBUG_TS("utime detected\n");
    general_timestamp_processor(0, filename, times, NULL, NULL, 0);

    // let the real sys_utime function run here
    hijack_pause(sys_utime);
    ret = sys_utime(filename, times);
    hijack_resume(sys_utime);

    return ret;
}

asmlinkage long n_sys_utimes(char __user *filename, struct timeval  __user *times)
{
    int ret;
    DEBUG_TS("utimes detected\n");
    hijack_pause(sys_utimes);
    ret = sys_utimes(filename, times);
    hijack_resume(sys_utimes);
    return ret;
}

asmlinkage long n_sys_futimesat (int dfd, char __user *filename, struct timeval __user *utimes)
{

    long ret;
    DEBUG_TS("futimesat detected\n");
    // do the heavy lefting here
    general_timestamp_processor(dfd, filename, NULL, utimes, NULL, 0);

    // let the real sys_utime function run here
    hijack_pause(sys_futimesat);
    ret = sys_futimesat(dfd, filename, utimes);
    hijack_resume(sys_futimesat);

    return ret;
}

asmlinkage long n_sys_utimensat (int dfd, char __user *filename, struct timespec __user *utimes, int flags)
{

    long ret;
    DEBUG_TS("utimensat detected\n");
    // do the heavy lefting here
    general_timestamp_processor(dfd, filename, NULL, NULL, utimes, flags);

    // let the real sys_utime function run here
    hijack_pause(sys_utimensat);
    ret = sys_utimensat(dfd, filename, utimes, flags);
    hijack_resume(sys_utimensat);

    return ret;
}

//This function takes the inputs of every system call that changes timestamps to reduce 
//the number of lines needed to process them
void general_timestamp_processor(int dfd,                       //add description of inputs here
                                 char __user *filename,
                                 struct utimbuf __user *times,
                                 struct timeval __user *vutimes,
                                 struct timespec __user *utimes,
                                 int flags)
{
    int interceptedcall;
    int err;
    char *fullpath;  //name of the file
    char *argument;  //argment for system call (may not be needed)
    char *callname;  //name of the system call
    char *newts;     //new timestamp
    char *oldts;     //old timestamp
    struct timespec *mytime; //current time
    struct kstat file_stat;

    mm_segment_t oldfs;

    mytime = kzalloc(sizeof(struct timespec), GFP_KERNEL);
    ktime_get_real_ts(mytime);
    
    
    oldts = kzalloc(64, GFP_KERNEL);
    newts = kzalloc(64, GFP_KERNEL);

    callname = kzalloc(10, GFP_KERNEL);
    //Look at the inputs to figure out which function called this
    if(times != NULL)
    {
        if(times->actime - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                mytime->tv_sec - times->actime <= ALLOWED_TIME_DIFFERENCE &&
                times->modtime - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                mytime->tv_sec - times->modtime <= ALLOWED_TIME_DIFFERENCE)
        {
            //No suspicious timechange has occured. Return early to save cycles.
            kfree(callname);
            kfree(mytime);
            return;
        }
        interceptedcall = UTIME_CALL;
        snprintf(callname , 10, "utime");
    }
    else if(vutimes != NULL)
    {
        if(vutimes[0].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                mytime->tv_sec - vutimes[0].tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                vutimes[1].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                mytime->tv_sec - vutimes[1].tv_sec <= ALLOWED_TIME_DIFFERENCE)
        {
            kfree(callname);
            kfree(mytime);
            return;
        }
        interceptedcall = FUTIMESAT_CALL;
        snprintf(callname , 10, "futimesat");
    }
    else if (utimes != NULL)
    {
        if(utimes[0].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                mytime->tv_sec - utimes[0].tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                utimes[1].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE &&
                mytime->tv_sec - utimes[1].tv_sec <= ALLOWED_TIME_DIFFERENCE)
        {
            kfree(callname);
            kfree(mytime);
            return;
        }
        interceptedcall = UTIMENSAT_CALL;
        snprintf(callname , 10, "utimensat");
    }
    else
    {
        /* System call made with null timestamp parameters.
         * Suspect file will automatically be set to current system time.
         * Nothing to do here*/
        kfree(callname);
        kfree(mytime);
        return;
    }

    // get the name of the file
    fullpath = kzalloc(DEFAULT_FILEPATH_SIZE, GFP_KERNEL);
    // if no file name was provided, use the file descriptor to the fullpath name
    if(filename == NULL)
    {
        if(vfs_fstat(dfd, &file_stat) >= 0)
        {
          snprintf(oldts, 64, "a: %lu.%lu m: %lu.%lu", file_stat.atime.tv_sec, 
                                                 file_stat.atime.tv_nsec,
                                                 file_stat.mtime.tv_sec,
                                                 file_stat.mtime.tv_nsec);
        }
        else 
          DEBUG_TS("vfs_fstat failed!\n");

        //readlink won't null terminate strings by default. kzalloc sets the whole thing to 0's
        argument = kzalloc(32, GFP_KERNEL);
        oldfs = get_fs();
        //Temporarily allow the usage of kernel memory addresses in system calls
        set_fs(KERNEL_DS);
        //Adapted from stackoverflow.com/questions/1188757/getting-filename-from-file-descriptor-in-c
        snprintf(argument, 32, "/proc/self/fd/%i", dfd);
        //The length is one less byte to ensure the string is null terminated
        err = sys_readlink(argument, fullpath, DEFAULT_FILEPATH_SIZE - 1);
        kfree(argument);
        set_fs(oldfs);
        if(err < 0)
        {
            DEBUG_TS("Error using readlink: %i\n", err);
        }


    }
    else  //if a file name was provided, get the full path
    {
        
        oldfs = get_fs();
        set_fs(KERNEL_DS);
        sys_getcwd(fullpath, DEFAULT_FILEPATH_SIZE);
        set_fs(oldfs);
        snprintf(fullpath, DEFAULT_FILEPATH_SIZE, "%s%s", fullpath, filename);

        if(vfs_stat(filename, &file_stat) >= 0)
        {
          snprintf(oldts, 64, "a: %lu.%lu m: %lu.%lu", file_stat.atime.tv_sec, 
                                                 file_stat.atime.tv_nsec,
                                                 file_stat.mtime.tv_sec,
                                                 file_stat.mtime.tv_nsec);
        }
        else
            DEBUG_TS("Error with vfs_stat\n");
    }
    write_log( (const char *) fullpath, DEFAULT_FILEPATH_SIZE);
    DEBUG_TS("%s used to set Timestamp for '%s' to", callname, fullpath);
    
    
    if(interceptedcall == UTIME_CALL)
    {
        snprintf(newts, 64, "a: %lu m:%lu",times->actime, times->modtime);
        DEBUG_TS(" %s\n", newts);
        
    }
    else if(interceptedcall == FUTIMESAT_CALL)
    {
        snprintf(newts, 64, "a: %lu.%lu m:%lu.%lu", vutimes[0].tv_sec, vutimes[0].tv_usec,
                                                    vutimes[1].tv_sec, vutimes[1].tv_usec);
        DEBUG_TS(" %s\n", newts);
    }
    else
    {
        snprintf(newts, 64, "a: %lu.%lu m:%lu.%lu", utimes[0].tv_sec, utimes[0].tv_nsec,
                                                    utimes[1].tv_sec, utimes[1].tv_nsec);
        DEBUG_TS(" %s\n", newts);
    }
    write_ts_log(callname, fullpath, oldts, newts);
    kfree(mytime);
    kfree(callname);
    kfree(fullpath);
    kfree(newts);
    kfree(oldts);
}

void hijack_stop_all_hookts(void)
{
    // undo the hooking
    hijack_stop(sys_utime);
    hijack_stop(sys_utimensat);
    hijack_stop(sys_futimesat);
    hijack_stop(sys_utimes);
}

void hijack_start_all_hookts(void)
{
    // grab the function pointer from the sys_call_table_decms
    // and start intercepting calls
    sys_utime = (void *)sys_call_table_decms[__NR_utime];
    sys_utimes = (void *)sys_call_table_decms[__NR_utimes];
    sys_utimensat = (void *)sys_call_table_decms[__NR_utimensat];
    sys_futimesat = (void *)sys_call_table_decms[__NR_futimesat];

    hijack_start(sys_utime, &n_sys_utime);
    hijack_start(sys_utimes, &n_sys_utimes);
    hijack_start(sys_utimensat, &n_sys_utimensat);
    hijack_start(sys_futimesat, &n_sys_futimesat);
}


void hookts_init ( void )
{
    DEBUG("Hooking sys_utime\n");

    hijack_start_all_hookts();

    //grab function pointers for other system calls needed by program
    sys_readlink = (void *)sys_call_table_decms[__NR_readlink];
    sys_getcwd = (void *)sys_call_table_decms[__NR_getcwd];

}

void hookts_exit ( void )
{
    DEBUG("Unhooking sys_utime\n");

    hijack_stop_all_hookts();
}
