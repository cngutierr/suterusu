#include "common.h"
#include <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/utime.h>
#include <linux/kthread.h>
//#include <linux/wait.h>
//#include <linux/circ_buf.h>
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


// pointers to the system call functions being hijacked
asmlinkage long (*sys_utime)(char __user *filename, struct utimbuf __user *times);
asmlinkage long (*sys_futimesat)(int dfd, char __user *filename, struct timeval __user *utimes);
asmlinkage long (*sys_utimensat)(int dfd, char __user *filename, struct timespec __user *utimes, int flags);

//pointers to system calls used by this program
asmlinkage long (*sys_readlink)(const char __user *path, char __user *buf, int bufsiz);
asmlinkage long (*sys_getcwd)(char __user *buf, unsigned long size);
asmlinkage long (*sys_clock_gettime)(clockid_t which_time, struct timespec __user *tp);

void general_timestamp_processor(int dfd, char __user *filename, struct utimbuf __user *times, struct timeval __user *vutimes, struct timespec __user *utimes, int flags);

ssize_t write_log(const char* entry, size_t entry_size)
{
    int ret;
    DEBUG("Start write_log\n");
    if(mutex_lock_interruptible(&write_lock))
      return -1; //fail
    DEBUG("Pushing into write_log %s\n", entry);
    ret = kfifo_in(&fifo_buf, entry, entry_size);
    mutex_unlock(&write_lock);
    to_log += 1;
    wake_up_interruptible(&log_event);
    return ret;
}

asmlinkage long n_sys_utime (char __user *filename, struct utimbuf __user *times)
{

    long ret;
    // do the heavy lefting here
    general_timestamp_processor(0, filename, times, NULL, NULL, 0);

    // let the real sys_utime function run here
    hijack_pause(sys_utime);
    ret = sys_utime(filename, times);
    hijack_resume(sys_utime);

    return ret;
}
asmlinkage long n_sys_futimesat (int dfd, char __user *filename, struct timeval __user *utimes)
{

    long ret;
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
    // do the heavy lefting here
    general_timestamp_processor(dfd, filename, NULL, NULL, utimes, flags);

    // let the real sys_utime function run here
    hijack_pause(sys_utimensat);
    ret = sys_utimensat(dfd, filename, utimes, flags);
    hijack_resume(sys_utimensat);

    return ret;
}

void general_timestamp_processor(int dfd, char __user *filename, struct utimbuf __user *times, struct timeval __user *vutimes, struct timespec __user *utimes, int flags)
{
    int interceptedcall;
    int err;
    void *fullpath;
    void *argument;
    void *callname;
    mm_segment_t oldfs;
    struct timespec *mytime;
    mytime = kzalloc(sizeof(struct timespec), GFP_KERNEL);
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    err = sys_clock_gettime(CLOCK_REALTIME, mytime);
    if(err < 0)
        {DEBUG_RW("Error using clock_gettime: %i\n", err);}
    set_fs(oldfs);
    callname = kzalloc(10, GFP_KERNEL);
    //DEBUG_RW(" current time:%ld\n", vutimes[0].tv_sec);
    if(times != NULL)
    {
        if(times->actime - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE && mytime->tv_sec - times->actime <= ALLOWED_TIME_DIFFERENCE
          && times->modtime - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE && mytime->tv_sec - times->modtime <= ALLOWED_TIME_DIFFERENCE)
        {
            //No suspicious timechange has occured. Return early to save cycles.
            kfree(callname);
            kfree(mytime);
            return;
        }
        interceptedcall = UTIME_CALL;
        snprintf((char *) callname , 10, "utime");
    }
    else if(vutimes != NULL)
    {
        if(vutimes[0].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE && mytime->tv_sec - vutimes[0].tv_sec <= ALLOWED_TIME_DIFFERENCE
           && vutimes[1].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE && mytime->tv_sec - vutimes[1].tv_sec <= ALLOWED_TIME_DIFFERENCE)
        {
            kfree(callname);
            kfree(mytime);
            return;
        }
        interceptedcall = FUTIMESAT_CALL;
        snprintf((char *) callname , 10, "futimesat");
    }
    else if (utimes != NULL)
    {
        if(utimes[0].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE && mytime->tv_sec - utimes[0].tv_sec <= ALLOWED_TIME_DIFFERENCE
           && utimes[1].tv_sec - mytime->tv_sec <= ALLOWED_TIME_DIFFERENCE && mytime->tv_sec - utimes[1].tv_sec <= ALLOWED_TIME_DIFFERENCE)
        {
            kfree(callname);
            kfree(mytime);
            return;
        }
        interceptedcall = UTIMENSAT_CALL;
        snprintf((char *) callname , 10, "utimensat");
    }
    else
    {
        //System call made with null timestamp parameters. Suspect file will automatically be set to current system time. Nothing to do here
        kfree(callname);
        kfree(mytime);
        return;
    }
    fullpath = kzalloc(DEFAULT_FILEPATH_SIZE, GFP_KERNEL);
    if(filename == NULL)
    {
        //readlink won't null terminate strings by default. kzalloc sets the whole thing to 0's
        argument = kzalloc(32, GFP_KERNEL);
        //Temporarily allow the usage of kernel memory addresses in system calls
        set_fs(KERNEL_DS);
        snprintf((char *) argument, 32, "/proc/self/fd/%i", dfd);
        //The length is one less byte to ensure the string is null terminated
        err = sys_readlink((char *) argument, (char *) fullpath, DEFAULT_FILEPATH_SIZE - 1);
        kfree(argument);
        set_fs(oldfs);
        if(err < 0)
            {DEBUG_RW("Error using readlink: %i\n", err);}
    }
    else
    {
            set_fs(KERNEL_DS);
            sys_getcwd( (char *) fullpath, DEFAULT_FILEPATH_SIZE);
            set_fs(oldfs);
            snprintf((char *) fullpath, DEFAULT_FILEPATH_SIZE, "%s%s", (char *) fullpath, filename);
    }
    write_log( (const char *) fullpath, 256);
    DEBUG_RW("%s used to set Timestamp for '%s' to", (char *) callname, (char *) fullpath);
    if(interceptedcall == UTIME_CALL)
        {DEBUG_RW(" a:%ld m:%ld\n",  times->actime, times->modtime);}
    else if(interceptedcall == FUTIMESAT_CALL)
        {DEBUG_RW(" a:%ld m:%ld\n",  vutimes[0].tv_sec, vutimes[1].tv_sec);}
    else
        {DEBUG_RW(" a:%ld m:%ld\n",  utimes[0].tv_sec, utimes[1].tv_sec);}
    kfree(callname);
    kfree(fullpath);
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
        DEBUG("Inside logger thread\n");
        /*
        head = smp_load_acquire(&log_circ_buf.head);
        tail = log_circ_buf.tail;
        
        DEBUG("Consumming data...\n");
        item = &log_circ_buf.buf[tail];
        smp_store_release(&log_circ_buf.tail,(tail + 1) & (BBB_SIZE - 1));
        */

        len = kfifo_out(&fifo_buf, log_entry, LOG_ENTRY_SIZE);
        log_entry[len] = '\0';
        if(logfile)
        {
            old_fs = get_fs();
            set_fs(get_ds());
            ret = vfs_write(logfile, log_entry, len, &pos);
            set_fs(old_fs);
            DEBUG("WROTE A LOG ENTRY: %i %s\n", len, log_entry);
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

void hookts_init ( void )
{
    int ret;
    DEBUG("Hooking sys_utime\n");
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
    
    // grab the function pointer from the sys_call_table
    // and start intercepting calls
    sys_utime = (void *)sys_call_table[__NR_utime];
    sys_utimensat = (void *)sys_call_table[__NR_utimensat];
    sys_futimesat = (void *)sys_call_table[__NR_futimesat];
    hijack_start(sys_utime, &n_sys_utime);
    hijack_start(sys_utimensat, &n_sys_utimensat);
    hijack_start(sys_futimesat, &n_sys_futimesat);

    //grab function pointers for other system calls needed by program
    sys_readlink = (void *)sys_call_table[__NR_readlink];
    sys_getcwd = (void *)sys_call_table[__NR_getcwd];
    sys_clock_gettime = (void *)sys_call_table[__NR_clock_gettime];
}

void hookts_exit ( void )
{
    DEBUG("Unhooking sys_utime\n");
    
    // undo the hooking 
    hijack_stop(sys_utime);
    hijack_stop(sys_utimensat);
    hijack_stop(sys_futimesat);
    
    //close the log file
    if(logfile)
       filp_close(logfile, NULL);
    
    // stop the logger thread
    to_log = 1;
    kthread_stop(logger_ts);
    
    //remove fifo
    kfifo_free(&fifo_buf);

}
