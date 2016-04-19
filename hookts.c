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
DEFINE_MUTEX(write_lock);
struct kfifo_rec_ptr_2 fifo_buf;

volatile unsigned long to_log = 0;
struct task_struct *logger_ts;
struct file *logfile;


// pointer to the system call functions
asmlinkage long (*sys_readlink)(const char __user *path, char __user *buf, int bufsiz);
asmlinkage long (*sys_utime)(char __user *filename, struct utimbuf __user *times);
asmlinkage long (*sys_futimesat)(int dfd, char __user *filename, struct timeval __user *utimes);
asmlinkage long (*sys_utimensat)(int dfd, char __user *filename, struct timespec __user *utimes, int flags);

// Do the heavy lifting in hook_utime... should be called in the n_sys_utime function 
// hook_utime should quickly identify that the timestamp is different than the current
// time and we should log the incident
void hook_utime(char __user *filename, struct utimbuf __user *times)
{
    /* Monitor/manipulate utime arguments here */
}
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

    #if __DEBUG_TS__  
    void *debug;
    int length = 0;
    //get the length of the filename from userspace
    length = strnlen_user(filename, NAME_MAX);
    if(length <= 0)
    {
       DEBUG_RW("Error: Failed to get the length of the file string\n");
    }
    else
    {
      // allocate some memory to copy the filename into kernel space
      debug = kmalloc(length, GFP_KERNEL);
      // failed to kmalloc, print some error message to dmesg
      if ( ! debug )
      {
        DEBUG_RW("ERROR: Failed to allocate %i bytes for sys_utime debugging\n", length);
      }
      else
      {
        // copy the filename for user space into kernel space
        if ( strncpy_from_user(debug, filename, length + 1) == 0)
        {
            DEBUG_RW("ERROR: Failed to copy %i bytes from user for sys_utime debugging\n", length);
            kfree(debug);
        }
        else
        {  
            // print out the file that was accessed or other 
            // debugging messages here.
            DEBUG_RW("Timestamp for '%s' was changed to", (char *) debug);
            //to do perhaps add a stat call here so we can see the current timestamp
            DEBUG_RW(" a:%ld m:%ld\n",  times->actime, times->modtime);
            
            write_log( (const char *) debug, length);

            kfree(debug);
        }
      }
    }
    #endif

    // do the heavy lefting here
    hook_utime(filename, times);

    // let the real sys_utime function run here
    hijack_pause(sys_utime);
    ret = sys_utime(filename, times);
    hijack_resume(sys_utime);

    return ret;
}
asmlinkage long n_sys_futimesat (int dfd, char __user *filename, struct timeval __user *utimes)
{

    long ret;

    #if __DEBUG_TS__
    void *debug;
    int length = 0;
    DEBUG_RW("Futimesat detected\n");
    //get the length of the filename from userspace
    length = strnlen_user(filename, NAME_MAX);
    if(length <= 0)
    {
       DEBUG_RW("Error: Failed to get the length of the file string\n");
    }
    else
    {
      // allocate some memory to copy the filename into kernel space
      debug = kmalloc(length, GFP_KERNEL);
      // failed to kmalloc, print some error message to dmesg
      if ( ! debug )
      {
        DEBUG_RW("ERROR: Failed to allocate %i bytes for sys_futimesat debugging\n", length);
      }
      else
      {
        // copy the filename for user space into kernel space
        if ( strncpy_from_user(debug, filename, length + 1) == 0)
        {
            DEBUG_RW("ERROR: Failed to copy %i bytes from user for sys_futimesat debugging\n", length);
            kfree(debug);
        }
        else
        {  
            // print out the file that was accessed or other 
            // debugging messages here.
            DEBUG_RW("Timestamp for '%s' was changed to", (char *) debug);
            //to do perhaps add a stat call here so we can see the current timestamp
            DEBUG_RW(" a:%ld m:%ld\n",  utimes[0].tv_sec, utimes[1].tv_sec);
            
            write_log( (const char *) debug, length);

            kfree(debug);
        }
      }
    }
    #endif
    // do the heavy lefting here
    //hook_utime(filename, times);

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
    void *debug;
    void *buf;
    void *argument;
    mm_segment_t oldfs;
    int length = 0;
    int err = 0;
    DEBUG_RW("Utimensat detected\n");
    //If the filename is null, we must be using a file descriptor
    if(filename == NULL)
    {
        //readlink won't null terminate strings by default. kzalloc sets the whole thing to 0's
        buf = kzalloc(256, GFP_KERNEL);
        argument = kzalloc(32, GFP_KERNEL);
        oldfs = get_fs();
        //Temporarily allow the usage of kernel memory addresses in system calls
        set_fs(KERNEL_DS);
	sys_readlink = (void *)sys_call_table[__NR_readlink];
        snprintf((char *) argument, 32, "/proc/self/fd/%i", dfd);
        //The length is one less byte to ensure the string is null terminated
        err = sys_readlink((char *) argument, (char *) buf, 255);
        set_fs(oldfs);
        if(err < 0)
        {
            DEBUG_RW("Readlink error: %i\n", err);
        }
        else
        {
            DEBUG_RW("Timestamp for '%s' was changed to", (char *) buf);
            //to do perhaps add a stat call here so we can see the current timestamp
            DEBUG_RW(" a:%ld m:%ld\n",  utimes[0].tv_sec, utimes[1].tv_sec);
            
            write_log( (const char *) buf, 256);
        }
        kfree(buf);
    }
    else
    {
        //get the length of the filename from userspace
        length = strnlen_user(filename, NAME_MAX);
        if(length <= 0)
        {
           DEBUG_RW("Error: Failed to get the length of the file string\n");
        }
        else
        {
          // allocate some memory to copy the filename into kernel space
          debug = kmalloc(length, GFP_KERNEL);
          // failed to kmalloc, print some error message to dmesg
          if ( ! debug )
          {
            DEBUG_RW("ERROR: Failed to allocate %i bytes for sys_utimensat debugging\n", length);
          }
          else
          {
            // copy the filename for user space into kernel space
            if ( strncpy_from_user(debug, filename, length + 1) == 0)
            {
                DEBUG_RW("ERROR: Failed to copy %i bytes from user for sys_utimensat debugging\n", length);
                kfree(debug);
            }
            else
            {  
                // print out the file that was accessed or other 
                // debugging messages here.
                DEBUG_RW("Timestamp for '%s' was changed to", (char *) debug);
                //to do perhaps add a stat call here so we can see the current timestamp
                DEBUG_RW(" a:%ld m:%ld\n",  utimes[0].tv_sec, utimes[1].tv_sec);
            
                write_log( (const char *) debug, length);

                kfree(debug);
            }
          }
        }
    }
    #endif
    // do the heavy lefting here
    //hook_utime(filename, times);

    // let the real sys_utime function run here
    hijack_pause(sys_utimensat);
    ret = sys_utimensat(dfd, filename, utimes, flags);
    hijack_resume(sys_utimensat);

    return ret;
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
