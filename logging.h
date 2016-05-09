#ifndef LOGGING_H
#define LOGGING_H

#include "common.h"
#include "kernel_syscall.h"
#include <linux/kfifo.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>
#include <linux/audit.h>
#include <linux/kthread.h>

#define LOG_FILE_STR "/tmp/.decms_log"
#define FIFO_SIZE 512
#define LOG_ENTRY_SIZE 512
#define LOG_BUF_SIZE 4096
extern struct file *logfile;
extern struct kfifo_rec_ptr_2 fifo_buf;
extern volatile unsigned long to_log;
extern struct task_struct *logger_ts;
extern bool decms_log_running;

void enable_logging(void);
void disable_logging(void);
int logger_thread(void *data);

/* helper method to write to log file (choose between hex and non-hex)*/
ssize_t _write_log(const char* entry, size_t entry_size, bool as_hex, const char *tag);
ssize_t write_log(const char* entry, size_t entry_size);
ssize_t write_hex_log(const char* entry, size_t entry_size);
//write buff to log... if multiple writes are needed for the same file, use the count 
ssize_t write_tagged_buf_log(const char* filename, int count, const char* entry, size_t entry_size);
ssize_t write_file_not_found_log(const char* filename, size_t filename_size);
/* logs timestamp changes for a file */
ssize_t write_ts_log(const char* sys_call_name,  // system call name
                const char *filename,            // file that is modified
                const char* oldts,               // old timestamp
                const char* newts);              // new timestamp

/* save the contents of the fd */
ssize_t write_sec_del_log(unsigned int fd);

/* filename whitelisting */
bool is_white_listed(const char * fullpath_name, size_t file_size);

/* check valid file format. We don't want pipes, etc */
bool is_valid_file(const char * fullpath_name, size_t file_size);

/* helper method to check a file name */
bool should_log(unsigned int fd);

#endif
