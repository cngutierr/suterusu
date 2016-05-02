#include "common.h"
#include <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/ktime.h>
#include <linux/utime.h>
#include <linux/kthread.h>
#include <linux/audit.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
//Needed to use readlink
//logger stuff

DECLARE_WAIT_QUEUE_HEAD(log_event);
/*#define BBB_SIZE 2048
char big_bad_buf[BBB_SIZE];
struct circ_buf log_circ_buf;*/
#define LOG_FILE_STR "/tmp/.decms_log"
#define FIFO_SIZE 512
#define LOG_ENTRY_SIZE 512
#define DEFAULT_FILEPATH_SIZE 256
#define UTIME_CALL 1
#define FUTIMESAT_CALL 2
#define UTIMENSAT_CALL 3
#define ALLOWED_TIME_DIFFERENCE 3
DEFINE_MUTEX(write_lock);
struct kfifo_rec_ptr_2 fifo_buf;
volatile unsigned long to_log = 0;
struct task_struct *logger_ts;
struct file *logfile;
bool decms_log_running = false;

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


ssize_t write_log(const char* entry, size_t entry_size)
{
    int ret, final_entry_size;
    char *final_entry;
    struct timespec *cur_time;
    char timestamp[22]; //stores the timestamp as a epoch string
    ret = 0;

    if(decms_log_running)
    {
        if(mutex_lock_interruptible(&write_lock))
            return -1; //fail
        //get the current time
        cur_time = kzalloc(sizeof(struct timespec), GFP_KERNEL);
        ktime_get_real_ts(cur_time);
        snprintf(timestamp, 22, "%lu.%lu", cur_time->tv_sec, cur_time->tv_nsec);

        //final entry will contain the current timestamp and whatever else was passed in
        final_entry_size = sizeof(timestamp) + entry_size + 2;
        final_entry = kzalloc(final_entry_size, GFP_KERNEL);
        snprintf(final_entry, final_entry_size, "%s\t%s\n", timestamp, entry);

        //push into fifo buf
        ret = kfifo_in(&fifo_buf, final_entry, final_entry_size);
        kfree(cur_time);
        kfree(final_entry);
        mutex_unlock(&write_lock);
        to_log += 1;
        wake_up_interruptible(&log_event);
    }
    else if(audit_enabled)
    {
        audit_log(NULL, GFP_KERNEL, AUDIT_KERNEL_OTHER,
                  "DecMS: %s", entry);
    }
    else
    {
      DEBUG("ERROR LOGGING: auditd and decms_logger not running!!\n");
    }

    return ret;
}
ssize_t write_ts_log(const char* sys_call_name, const char* filename, const char* oldts, const char* newts)
{
    char buf[512];
    int len;
    len = snprintf(buf, 512, "callname=%s file=%s oldts=(%s) newts=(%s)", 
                        sys_call_name, filename, oldts, newts);   
    
    return write_log(buf, len);
}

asmlinkage long n_sys_utime (char __user *filename, struct utimbuf __user *times)
{

    long ret;
#if __DEBUG_TS__
    DEBUG_RW("utime detected\n");
#endif
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
#if __DEBUG_TS__
    DEBUG_RW("utimes detected\n");
#endif
    hijack_pause(sys_utimes);
    ret = sys_utimes(filename, times);
    hijack_resume(sys_utimes);
    return ret;
}

asmlinkage long n_sys_futimesat (int dfd, char __user *filename, struct timeval __user *utimes)
{

    long ret;
#if __DEBUG_TS__
    DEBUG_RW("futimesat detected\n");
#endif
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
#if __DEBUG_TS__
    DEBUG_RW("utimensat detected\n");
#endif
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
          DEBUG_RW("vfs_fstat failed!\n");

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
            DEBUG_RW("Error using readlink: %i\n", err);
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
            DEBUG("Error with vfs_stat\n");
    }
    write_log( (const char *) fullpath, DEFAULT_FILEPATH_SIZE);
    DEBUG_RW("%s used to set Timestamp for '%s' to", callname, fullpath);
    
    
    if(interceptedcall == UTIME_CALL)
    {
        snprintf(newts, 64, "a: %lu m:%lu",times->actime, times->modtime);
        DEBUG_RW(" %s\n", newts);
        
    }
    else if(interceptedcall == FUTIMESAT_CALL)
    {
        snprintf(newts, 64, "a: %lu.%lu m:%lu.%lu", vutimes[0].tv_sec, vutimes[0].tv_usec,
                                                    vutimes[1].tv_sec, vutimes[1].tv_usec);
        DEBUG_RW(" %s\n", newts);
    }
    else
    {
        snprintf(newts, 64, "a: %lu.%lu m:%lu.%lu", utimes[0].tv_sec, utimes[0].tv_nsec,
                                                    utimes[1].tv_sec, utimes[1].tv_nsec);
        DEBUG_RW(" %s\n", newts);
    }
    write_ts_log(callname, fullpath, oldts, newts);
    kfree(mytime);
    kfree(callname);
    kfree(fullpath);
    kfree(newts);
    kfree(oldts);
}

int logger_thread(void *data)
{
    char log_entry[LOG_ENTRY_SIZE];
    int ret, len;
    loff_t pos = 0;
    mm_segment_t old_fs;

    while(1)
    {

        wait_event_interruptible(log_event, (to_log > 0));

        len = kfifo_out(&fifo_buf, log_entry, LOG_ENTRY_SIZE);
        log_entry[len] = '\0';
            DEBUG("Processing log entry with decms_logger\n");
            if(logfile)
            {
                old_fs = get_fs();
                set_fs(get_ds());
                ret = vfs_write(logfile, log_entry, len, &pos);
                set_fs(old_fs);
                //DEBUG("WROTE A LOG ENTRY: %i %s\n", len, log_entry);
            }

        to_log -= 1;
        if(kthread_should_stop() == true)
        {
            DEBUG("Stopping DecMS logger...\n");
            break;
        }
    }

    return 0;
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
    int ret;
    DEBUG("Hooking sys_utime\n");

    /*
     * At the start of the program, check to see if auditd is running
     * if it is not running, use the decms logger. If auditd is turned on
     * after this point, we will still use the decms logger.
     */
    if(!audit_enabled)
    {
        DEBUG("'auditd' is not running... use 'dems_logger'");
        logfile = filp_open(LOG_FILE_STR, O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
        if(!logfile)
        {
            DEBUG("Failed to open logfile '%s'\n", LOG_FILE_STR);
        }
        //log_circ_buf.buf = big_bad_buf;
        // init the fifo buf
        ret = kfifo_alloc(&fifo_buf, FIFO_SIZE, GFP_KERNEL);
        if(ret)
        {
            DEBUG("Error allocating fifo log buff");
        }

        logger_ts = kthread_run(logger_thread, NULL, "decms_logger");
        decms_log_running = true;
    }
    else
    {
        DEBUG("'auditd' will log events.\n");
    }
    hijack_start_all_hookts();

    //grab function pointers for other system calls needed by program
    sys_readlink = (void *)sys_call_table_decms[__NR_readlink];
    sys_getcwd = (void *)sys_call_table_decms[__NR_getcwd];

}

void hookts_exit ( void )
{
    DEBUG("Unhooking sys_utime\n");


    if(decms_log_running)
    {
        //close the log file
        if(logfile)
            filp_close(logfile, NULL);
        // stop the logger thread
        to_log = 1;
        kthread_stop(logger_ts);
        decms_log_running = false;
    }
    hijack_stop_all_hookts();

    //remove fifo
    kfifo_free(&fifo_buf);

}
