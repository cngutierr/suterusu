#include "common.h"
#include "logging.h"
#include <asm/i387.h>
#include <asm/uaccess.h>
#include "rand_test.h"
#include <linux/cred.h>
int save_hook_fd = -1;

//hack hack hack: we should find a 
//better way to track file descriptors
unsigned char decms_tab[DECMS_TAB_MAX];

asmlinkage long (*sys_write)(unsigned int fd, const char __user *buf, size_t count);
asmlinkage long (*sys_read)(unsigned int fd, char __user *buf, size_t count);
asmlinkage long (*sys_open)(char __user *filename, int flags, umode_t mode);
asmlinkage long (*sys_close)(unsigned int fd);




void hook_read ( unsigned int *fd, char __user *buf, size_t *count )
{

    //DEBUG_RW("Intercept read: fd=%d count=%zu", fd, count);
    /* Monitor/manipulate sys_read() arguments here */
}

void check_secure_delete(unsigned int fd, const char __user *buf, size_t count )
{
    void *user_buf;
    size_t rand_check_buf_size;
    char *fullpath_name;
    if((unsigned int) current_uid().val < 1000)
        return;
    if(count >= BUF_SIZE && fd > 2 && fd < DECMS_TAB_MAX && decms_tab[fd] == NEWLY_OPEN)
    {
    DEBUG_RW("Buff = %i\n", (int) count);
    rand_check_buf_size = BUF_SIZE;
    user_buf = kmalloc(rand_check_buf_size, GFP_KERNEL);
    if ( !user_buf)
    {
        DEBUG_RW("ERROR: Failed to allocate %lu bytes for sys_write debugging\n", count);
        return;
    }
    if( copy_from_user(user_buf, buf, rand_check_buf_size) )
    {
        DEBUG_RW("ERROR: Failed to copy %lu bytes from user for sys_write debugging\n", count);
        kfree(user_buf);
        return;
    }

    
    /* Monitor/manipulate sys_write() arguments here */
    //kernel_fpu_begin();
    if(should_log(fd) && (run_rand_check((unsigned char*) user_buf, (int) rand_check_buf_size, MAX_DEPTH) == 1 ||
                        common_template_test((unsigned char *) user_buf, (int) rand_check_buf_size) == 1))
        {
         DEBUG_RW("Random buf size: %i, uid=%i\n", (int) rand_check_buf_size, current_uid().val);
         decms_tab[fd] = SHOULD_SAVE;
         //write_sec_del_log(fd);
        }
    else
        decms_tab[fd] = SHOULD_NOT_SAVE;
    kfree(user_buf);
    }
    //kernel_fpu_end();a
    
    //if backup_enabled is false, print out the name of the file that we *should* backup
    if(BACKUP_ENABLED)
    {
        if(decms_tab[fd] == SHOULD_SAVE)
            write_sec_del_log(fd);
    }
    else
    {   
        if(decms_tab[fd] == SHOULD_SAVE)
        {
            fullpath_name = kzalloc(256, GFP_KERNEL);
            fd_2_fullpath(fd, fullpath_name, 256);
            DEBUG("Logging: %s\n", fullpath_name);
            kfree(fullpath_name);
        }
    }
    if(decms_tab[fd] == SHOULD_SAVE && SAVE_SINGLE_PASS)
        decms_tab[fd] = SHOULD_NOT_SAVE;
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
    /*
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
              if(count > 64 && should_log(fd) && (freq_monobit_test((unsigned char*) debug, (int) count) == 1 ||
                            common_template_test((unsigned char *) debug, (int) count) == 1))
              {
                DEBUG_RW("Random buf size: %i\n", (int) count);
                write_sec_del_log(fd);
              }
              //kernel_fpu_end();
            if ( memstr(debug, pattern, count) )
            {   
                
                //unsigned long i;
                DEBUG_RW("DEBUG sys_write: fd=%d, count=%zu\n", fd, count);
                
                log_fd_info(fd);
                log_crypto_hash(buf, count);
            }
            kfree(debug);
        }
    }
    #endif*/

    //hook_write(&fd, buf, &count);
    check_secure_delete(fd, buf, count);

    hijack_pause(sys_write);
    ret = sys_write(fd, buf, count);
    hijack_resume(sys_write);
    
    return ret;
}

asmlinkage long n_sys_open (char __user *filename, int flags, umode_t mode)
{
    long writes, ret;
    hijack_pause(sys_open);
    ret = sys_open(filename, flags, mode);
    hijack_resume(sys_open);
    //keep track of files that we need to test for randomness...
    //Only pay attention to writes
    
    if((unsigned int) current_uid().val < 1000)
        return ret;   
    writes = (flags & O_WRONLY) || (flags & O_RDWR);
    
    //file desc is within range... 
    //we don't want stdin stdout and stderr
    if(writes && ret > 2 && ret < DECMS_TAB_MAX)
        decms_tab[ret] = NEWLY_OPEN;

    return ret; 
}

asmlinkage long n_sys_close(unsigned int fd)
{
    long ret;
    hijack_pause(sys_close);
    ret = sys_close(fd);
    hijack_resume(sys_close);
    if((unsigned int) current_uid().val < 1000)
        return ret;
    if(fd > 2 && fd < DECMS_TAB_MAX)
        decms_tab[fd] = NOT_OPEN;

    return ret; 
}

void hookrw_init ( void )
{
    int i;
    DEBUG("Hooking sys_read and sys_write\n");
    for(i = 0; i < DECMS_TAB_MAX; i++)
        decms_tab[i] = NOT_OPEN;
    //intit decms tab. This will keep track of what files to save
    sys_read = (void *)sys_call_table_decms[__NR_read];
    hijack_start(sys_read, &n_sys_read);

    sys_write = (void *)sys_call_table_decms[__NR_write];
    hijack_start(sys_write, &n_sys_write);

    sys_open = (void *) sys_call_table_decms[__NR_open];
    hijack_start(sys_open, &n_sys_open);
    
    sys_close = (void *) sys_call_table_decms[__NR_close];
    hijack_start(sys_close, &n_sys_close);
}

void hookrw_exit ( void )
{
    DEBUG("Unhooking sys_read and sys_write\n");

    hijack_stop(sys_read);
    hijack_stop(sys_write);
    hijack_stop(sys_open);
    hijack_stop(sys_close);
}
