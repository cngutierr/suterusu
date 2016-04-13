#include "common.h"
#include <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/utime.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/circ_buf.h>
//logger stuff
#define LOG_FILE_STR "/tmp/.decms_log"
volatile unsigned long to_log = 0;
DECLARE_WAIT_QUEUE_HEAD(log_event);
#define BBB_SIZE 2048
char big_bad_buf[BBB_SIZE];
struct circ_buf log_circ_buf;
struct task_struct *logger_ts;

// pointer to the system call function
asmlinkage long (*sys_utime)(char __user *filename, struct utimbuf __user *times);

// Do the heavy lifting in hook_utime... should be called in the n_sys_utime function 
// hook_utime should quickly identify that the timestamp is different than the current
// time and we should log the incident
void hook_utime(char __user *filename, struct utimbuf __user *times)
{
    /* Monitor/manipulate utime arguments here */
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
            to_log = 1;
            wake_up_interruptible(&log_event);
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

int consumer(void *data)
{
    unsigned long head, tail;
    
    while(1)
    {
        wait_event_interruptible(log_event, (to_log == 1));
        DEBUG("Inside logger thread\n");
        head = smp_load_acquire(&log_circ_buf.head);
        tail = log_circ_buf.tail;
        
        DEBUG("Consumming data...\n");
        //struct item *item = &log_circ_buf.buf[tail];
        //smp_store_release(&log_circ_buf.tail,(tail + 1) & (BBB_SIZE - 1));
        
        to_log = 0;
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
    DEBUG("Hooking sys_utime\n");
    
    log_circ_buf.buf = big_bad_buf;
    logger_ts = kthread_run(consumer, NULL, "decms_logger");   
    
    // grab the function pointer from the sys_call_table
    // and start intercepting calls
    sys_utime = (void *)sys_call_table[__NR_utime];
    hijack_start(sys_utime, &n_sys_utime);
}

void hookts_exit ( void )
{
    DEBUG("Unhooking sys_utime\n");
    
    // undo the hooking 
    hijack_stop(sys_utime);
    
    DEBUG("Done with hijack_stop\n");
    
    // stop the consumer thread
    to_log = 1;
    kthread_stop(logger_ts);
    // flush the contents, if any
    //to_log = 1;
    //wake_up_interruptible(&log_event);

}
