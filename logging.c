#include "logging.h"
#include <linux/random.h>
#include <asm/unistd.h>
#include "3rdparty/tools.h"
DECLARE_WAIT_QUEUE_HEAD(log_event);
DEFINE_MUTEX(write_lock);
struct kfifo_rec_ptr_2 fifo_buf;
volatile unsigned long to_log = 0;
struct task_struct *logger_ts;
struct file *logfile;
bool decms_log_running = false;


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
        DEBUG_LOG("'auditd' is not running... use 'dems_logger'");
        logfile = filp_open(LOG_FILE_STR, O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
        if(!logfile)
        {
            DEBUG_LOG("Failed to open logfile '%s'\n", LOG_FILE_STR);
        }
        //log_circ_buf.buf = big_bad_buf;
        // init the fifo buf
        ret = kfifo_alloc(&fifo_buf, FIFO_SIZE, GFP_KERNEL);
        if(ret)
        {
            DEBUG_LOG("Error allocating fifo log buff");
        }

        logger_ts = kthread_run(logger_thread, NULL, "decms_logger");
        decms_log_running = true;
    }
    else
    {
        DEBUG_LOG("'auditd' will log events.\n");
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
            DEBUG_LOG("Processing log entry with decms_logger\n");
            if(logfile)
            {
                old_fs = get_fs();
                set_fs(get_ds());
                ret = vfs_write(logfile, log_entry, len, &pos);
                set_fs(old_fs);
                //DEBUG_LOG("WROTE A LOG ENTRY: %i %s\n", len, log_entry);
            }

        to_log -= 1;
        if(kthread_should_stop() == true)
        {
            DEBUG_LOG("Stopping DecMS logger...\n");
            break;
        }
    }
    return 0;
}

/* Lazy... don't want to refactor =( */
ssize_t write_log(const char* entry, size_t entry_size)
{
    return _write_log(entry, entry_size, 0, NULL);
}

struct hex_log_data
{
    size_t entry_size;
    const char* entry;
};

/*Simple hexified log entry*/
ssize_t write_hex_log(const char* entry, size_t entry_size)
{
   if(THREAD_LOGGING)
   {
     /* create the structure that will hold the pointer
      * and size of the log data.
      */
      struct hex_log_data data;
      data.entry_size = entry_size;
      data.entry = entry;
      kthread_run(auditd_thread_logging, &data, "audit_logger");
      return 0;
    }    
   else
   {    
    return _write_log(entry, entry_size, 1, NULL);
   }
}

int auditd_thread_logging(void* in_data)
{
    /*
     * parse the input data, call _write_log, then exit
     */
    struct hex_log_data* data = (struct hex_log_data *) in_data;
    _write_log(data->entry, data->entry_size, 1, NULL);
    //do_exit(0);
    return 0;
}
ssize_t write_tagged_buf_log(const char *tag, unsigned long count, unsigned long serial, 
                const char* entry, size_t entry_size)
{  
   int size = strlen(tag) + 2 + 64;
   ssize_t ret;
   char *updated_tag = kzalloc(size, GFP_ATOMIC);
   snprintf(updated_tag, size, "%s (%lu) (%lu)", tag, serial, count);
   ret = _write_log(entry, entry_size, 1, updated_tag);
   kfree(updated_tag);
   return ret;
}

ssize_t _write_log(const char* entry, size_t entry_size, bool as_hex, const char* tag)
{
    int ret, final_entry_size;
    char *final_entry;
    struct timespec *cur_time;
    char timestamp[22]; //stores the timestamp as a epoch string
    char *hexified_entry;
    int hexified_entry_size;
    int hex_count;
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
        snprintf( final_entry, final_entry_size, "%s\t%s\n", timestamp, entry);

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
        if(as_hex)
        {
            // hexify the input
            hexified_entry_size = entry_size*2 + strlen(tag) + 16;
            //DEBUG_LOG("kalloc memory of size = %i\n", (int) hexified_entry_size);
            hexified_entry = kmalloc(hexified_entry_size, GFP_KERNEL);
            //we should check here if alloc was successful
            hex_count = hexify( (const uint8_t *)entry, entry_size, hexified_entry, hexified_entry_size);
            if(tag)
                audit_log(NULL, GFP_KERNEL, AUDIT_KERNEL_OTHER,
                  "DecMS={%s=[%s]}", tag, hexified_entry);
            else
                audit_log(NULL, GFP_KERNEL, AUDIT_KERNEL_OTHER,
                  "DecMS={%s}", hexified_entry);

            kfree(hexified_entry);
        }
        else
        {
            audit_log(NULL, GFP_KERNEL, AUDIT_KERNEL_OTHER,
                  "DecMS: %s", entry);
        }
    }
    else
    {
      DEBUG_LOG("ERROR LOGGING: auditd and decms_logger not running!!\n");
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


ssize_t write_sec_del_log(unsigned int fd)
{
    ssize_t ret;
    char *fullpath_name;
    char *file_content_buf;
    mm_segment_t old_fs;
    struct file *save_file;
    loff_t pos = 0;
    struct kstat file_stat;
    unsigned int out_size;
    unsigned long count = 0;
    unsigned long serial;
    
    fullpath_name = kzalloc(256, GFP_KERNEL);
    ret = fd_2_fullpath(fd, fullpath_name, 256);
    get_random_bytes(&serial, sizeof(serial));
    if(ret < 0)
    {
        DEBUG_LOG("Error using readlink: %i\n", (int)ret);
        kfree(fullpath_name);
        return -1;
    }
    else
    {
     DEBUG_LOG("logging: %s\n", fullpath_name);
     //write_log(fullpath_name, 256);
    }
    
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    
    if(vfs_fstat(fd, &file_stat) == -1)
    {
        DEBUG_LOG("Failed to open vfs_fstat in logging.c \n");
        set_fs(old_fs);
        kfree(fullpath_name);
        return -1;
    }

    DEBUG_LOG("Size of file = %i\nblksize=%lu\nblocks=%lu\nmode=%x\n", (int) file_stat.size, file_stat.blksize,(unsigned long) file_stat.blocks, (unsigned int)file_stat.mode);
    if(S_ISREG(file_stat.mode))
    {
        DEBUG_LOG("FILE IS REG\n");
    }
    if(file_stat.size < LOG_BUF_SIZE)
    {
        out_size = file_stat.size;
        file_content_buf = kmalloc(out_size, GFP_KERNEL);
    }
    else
    {
        out_size = LOG_BUF_SIZE;
        file_content_buf = kmalloc(LOG_BUF_SIZE, GFP_KERNEL);
    }
    save_file = filp_open(fullpath_name, O_RDONLY, 0);
    
    if(fd >= 0 && file_stat.size > 0)
    {       
        while(vfs_read(save_file, file_content_buf, out_size, &pos) >= 0)
        {
         // DEBUG_LOG("File found, write out buffer, offset = %i, count = %lu, serial = %lu\n",
         //                (int)pos, count, serial);
         ret = write_tagged_buf_log(fullpath_name, count, serial, file_content_buf, out_size);
         
         if(file_stat.size == pos)
            break;
         
         if(file_stat.size - pos < LOG_BUF_SIZE)
         {
            DEBUG_LOG("last loop\n");
            out_size = file_stat.size - pos;
         }
         count++;
        }
        /*else
        {
         DEBUG_LOG("'%s' not found... log incident", fullpath_name);
         ret = write_file_not_found_log(fullpath_name, 256);
        }*/
    }
    else
    {   
        DEBUG_LOG("invalid file or is a pipe\n");
        set_fs(old_fs);
        kfree(fullpath_name);
        kfree(file_content_buf);
        return ret;
    }
    filp_close(save_file,0);
    set_fs(old_fs);
    kfree(fullpath_name);
    kfree(file_content_buf);
    
    return ret;
}

ssize_t write_file_not_found_log(const char* filename, size_t filename_size)
{
   char* log_buf;
   int log_buf_size = filename_size + 256;
   ssize_t ret;
   log_buf = kzalloc(log_buf_size, GFP_KERNEL);
   snprintf(log_buf, log_buf_size, "Buffer writing to '%s' contains random binary", filename);
   ret = write_log(log_buf, log_buf_size);
   kfree(log_buf);
   return ret;
}

bool is_white_listed(const char* fullpath_name, size_t filename_size)
{
    /*
     * TODO make this part of the code cleaner. We should just itterate
     * through a whitelist of words
     */
    if(memstr(fullpath_name, "/var/log/audit/audit.log", filename_size) ||
        memstr(fullpath_name, "/var/log/auth.log",filename_size) ||
        memstr(fullpath_name, "/var/log/kern.log", filename_size) ||
        memstr(fullpath_name, "/var/log/syslog", filename_size) ||
        memstr(fullpath_name, "/dev/pts/", filename_size) ||
        memstr(fullpath_name, "/dev/null", filename_size))
    { 
        //DEBUG_LOG("'%s' is a white listed file!\n", fullpath_name);
        return true;
    }
    return false;
}


bool is_valid_file(const char * fullpath_name, size_t file_size)
{
    if(file_size > 0 && fullpath_name[0] != '/')
    {
        //DEBUG_LOG("'%s' is not a normal file!\n", fullpath_name);
        return false;
    }
    return true;
}

bool should_log(unsigned int fd)
{
    ssize_t ret;                // for return values
    char *fullpath_name;        // full path name of the file

    fullpath_name = kzalloc(256, GFP_KERNEL);
    ret = fd_2_fullpath(fd, fullpath_name, 256);
    
    // could not read the simlink
    
    if(ret < 0)
    {
       DEBUG_LOG("Error using readlink\n");
       ret = false;
       goto cleanup;
    }
    //if the file is valid and not whitelisted, we should check
    else if(is_valid_file(fullpath_name, 255) && !is_white_listed(fullpath_name, 255))
    {       
        ret = true;
        goto cleanup;
    }
    else
    {
        ret = false;
        goto cleanup;
    }

cleanup:
       kfree(fullpath_name);
       return ret;
}
