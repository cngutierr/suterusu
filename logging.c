#include "logging.h"
DECLARE_WAIT_QUEUE_HEAD(log_event);
DEFINE_MUTEX(write_lock);
struct kfifo_rec_ptr_2 fifo_buf;
volatile unsigned long to_log = 0;
struct task_struct *logger_ts;
struct file *logfile;
bool decms_log_running = false;

extern asmlinkage long (*sys_readlink)(const char __user *path, char __user *buf, int bufsiz);

//extern asmlinkage long (*sys_read)(unsigned int fd, char __user *buf, size_t bufsiz);
//asmlinkage long (*sys_read)(unsigned int fd, char __user *buf, size_t bufsiz);
//asmlinkage long (*sys_open)(const char __user *filename, int flags, umode_t mode);
//asmlinkage long (*sys_close)(unsigned int fd);

void enable_logging(void)
{
    int ret;
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
}

void disable_logging(void)
{
    if(decms_log_running)
    {
        //close the log file
        if(logfile)
            filp_close(logfile, NULL);
        // stop the logger thread
        to_log = 1;
        kthread_stop(logger_ts);
        decms_log_running = false;
        kfifo_free(&fifo_buf);
    }
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

ssize_t write_log(const char* entry, size_t entry_size)
{
    int ret, final_entry_size;
    char *final_entry;
    struct timespec *cur_time;
    char timestamp[22]; //stores the timestamp as a epoch string
    ret = 0;
    
    if(entry_size < 0)
    {
        return -1;
    }


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


ssize_t write_fd_log(unsigned int fd)
{
    ssize_t ret;
    char *sim_link;
    char *fullpath_name;
    char *file_content_buf;
    mm_segment_t old_fs;
    struct file *save_file;
    loff_t pos = 0;

    sys_readlink = (void *) sys_call_table_decms[__NR_readlink];   
    
    //get the filename 
    sim_link = kzalloc(32, GFP_KERNEL);
    fullpath_name = kzalloc(256, GFP_KERNEL);
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    snprintf(sim_link, 32, "/proc/self/fd/%i", fd);
    ret = sys_readlink(sim_link, fullpath_name, 255);
    
    if(ret < 0)
    {
        DEBUG("Error using readlink\n");
        kfree(sim_link);
        kfree(fullpath_name);
        return -1;
    }
    
    if(fullpath_name[0] != '/')
    {
        // this is not a file on the disk
        set_fs(old_fs);
        kfree(sim_link);
        kfree(fullpath_name);
        return 0;
    }
    else
    {
        DEBUG("logging: %s\n", fullpath_name);
        write_log(fullpath_name, 256);
    }
    
    //save the file 
    file_content_buf = kzalloc(4096, GFP_KERNEL);
    save_file = filp_open(fullpath_name, O_RDONLY, 0);
    if(fd >= 0)
    {       
        if(vfs_read(save_file, file_content_buf, 4096, &pos) >= 0)
        {
         DEBUG("File found, write out buffer, offset = %i\n", (int)pos);
         write_log(file_content_buf, 4096);
        } 
    }
    else
    {   
        DEBUG("invalid file or is a pipe\n");
        set_fs(old_fs);
        kfree(sim_link);
        kfree(fullpath_name);
        kfree(file_content_buf);
        return ret;
    }
    filp_close(save_file,0);
    set_fs(old_fs);
    kfree(sim_link);
    kfree(fullpath_name);
    kfree(file_content_buf);
    
    return ret;
}
