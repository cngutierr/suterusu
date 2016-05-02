#ifndef LOGGING_H
#define LOGGING_H

#include "common.h"
#include <linux/kfifo.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>
#include <linux/audit.h>
#include <linux/kthread.h>

#define LOG_FILE_STR "/tmp/.decms_log"
#define FIFO_SIZE 512
#define LOG_ENTRY_SIZE 512
extern struct file *logfile;
extern struct kfifo_rec_ptr_2 fifo_buf;
extern volatile unsigned long to_log;
extern struct task_struct *logger_ts;
extern bool decms_log_running;

void enable_logging(void);
void disable_logging(void);
int logger_thread(void *data);

/* helper method to write to log file */
ssize_t write_log(const char* entry, size_t entry_size);

/* logs timestamp changes for a file */
ssize_t write_ts_log(const char* sys_call_name,  // system call name
                const char *filename,            // file that is modified
                const char* oldts,               // old timestamp
                const char* newts);              // new timestamp

/* save the contents of the fd */
ssize_t write_fd_log(unsigned int fd);

#endif
