#include "common.h"
#include <asm/i387.h>
#include <asm/uaccess.h>
#include "rand_test.h"
int save_hook_fd = -1;
asmlinkage long (*sys_write)(unsigned int fd, const char __user *buf, size_t count);
asmlinkage long (*sys_read)(unsigned int fd, char __user *buf, size_t count);


void hook_read ( unsigned int *fd, char __user *buf, size_t *count )
{

    //DEBUG_RW("Intercept read: fd=%d count=%zu", fd, count);
    /* Monitor/manipulate sys_read() arguments here */
}

void hook_write ( unsigned int *fd, const char __user *buf, size_t *count )
{
    /* Monitor/manipulate sys_write() arguments here */
}


asmlinkage long n_sys_read ( unsigned int fd, char __user *buf, size_t count )
{
    long ret;
    char pattern[6] = {0x7f, 0x45, 0x4c, 0x46, 0x02, 0x00};

    #if __DEBUG_RW__
    void *debug;

    debug = kmalloc(count, GFP_KERNEL);
    if ( ! debug )
    {
        DEBUG_RW("ERROR: Failed to allocate %lu bytes for sys_read debugging\n", count);
    }
    else
    {
        if ( copy_from_user(debug, buf, count) )
        {
            DEBUG_RW("ERROR: Failed to copy %lu bytes from user for sys_read debugging\n", count);
            kfree(debug);
        }
        else
        {     
              //DEBUG_RW("Intercept reading: fd=%d count=%zu", fd, count);
              if (memstr(debug, pattern, count))
              {   
                //unsigned long i;
                DEBUG_RW("DEBUG sys_read: fd=%d, count=%zu, buf=\n", fd, count);
                /*for ( i = 0; i < count; i++ )
                {
                    DEBUG_RW("%x", *((unsigned char *)debug + i));
                    if(i > 32)
                       break;
                }
                DEBUG_RW("\n");*/
                log_fd_info(fd);
              }
            kfree(debug);
        }
    }
    #endif

    hook_read(&fd, buf, &count);

    hijack_pause(sys_read);
    ret = sys_read(fd, buf, count);
    hijack_resume(sys_read);

    return ret;
}

asmlinkage long n_sys_write ( unsigned int fd, const char __user *buf, size_t count )
{
    long ret;

    #if __DEBUG_RW__
    void *debug;
    char pattern[6] = {0x7f, 0x45, 0x4c, 0x46, 0x02, 0x00};
    debug = kmalloc(count, GFP_KERNEL);
    if ( ! debug )
    {
        DEBUG_RW("ERROR: Failed to allocate %lu bytes for sys_write debugging\n", count);
    }
    else
    {
        if ( copy_from_user(debug, buf, count) )
        {
            DEBUG_RW("ERROR: Failed to copy %lu bytes from user for sys_write debugging\n", count);
            kfree(debug);
        }
        else
        {  
              
              //kernel_fpu_begin();
              if(count > 0 && freq_monobit_test((unsigned char*) debug, (int) count) == 1)
              {
                DEBUG_RW("Random buf: %i\n", count);
              }
              //kernel_fpu_end();
            if ( memstr(debug, pattern, count) )
            {   
                
                //unsigned long i;
                DEBUG_RW("DEBUG sys_write: fd=%d, count=%zu\n", fd, count);
                
                /*
                for ( i = 0; i < count; i++ )
                {
                    DEBUG_RW("%x", *((unsigned char *)debug + i));
                    if(i > 32)
                       break;
                }
                DEBUG_RW("\n");
                //log_fd_info(fd);*/
                log_fd_info(fd);
                log_crypto_hash(buf, count);
            }
            kfree(debug);
        }
    }
    #endif

    hook_write(&fd, buf, &count);

    hijack_pause(sys_write);
    ret = sys_write(fd, buf, count);
    hijack_resume(sys_write);

    return ret;
}

void hookrw_init ( void )
{
    DEBUG("Hooking sys_read and sys_write\n");

    sys_read = (void *)sys_call_table_decms[__NR_read];
    hijack_start(sys_read, &n_sys_read);

    sys_write = (void *)sys_call_table_decms[__NR_write];
    hijack_start(sys_write, &n_sys_write);
}

void hookrw_exit ( void )
{
    DEBUG("Unhooking sys_read and sys_write\n");

    hijack_stop(sys_read);
    hijack_stop(sys_write);
}
