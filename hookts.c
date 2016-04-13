#include "common.h"
#include <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/utime.h>

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

void hookts_init ( void )
{
    DEBUG("Hooking sys_utime\n");

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
}
