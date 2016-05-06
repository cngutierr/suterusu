#include "kernel_syscall.h"

asmlinkage long (*sys_readlink)(const char __user *path, char __user *buf, int bufsiz);
asmlinkage long (*sys_getcwd)(char __user *buf, unsigned long size);


long kernel_readlink(const char __user *path, char __user *buf, int bufsize)
{
    sys_readlink = (void *)sys_call_table_decms[__NR_readlink];
    return sys_readlink(path, buf, bufsize);
}

long kernel_getcwd(char __user *buf, unsigned long size)
{
    sys_getcwd = (void *)sys_call_table_decms[__NR_getcwd];
    return sys_getcwd(buf, size);
}

int fd_2_fullpath(unsigned int fd, char *fullpath_name, int fullpath_size)
{
    int ret;
    char *sim_link;
    mm_segment_t old_fs;        // saves the old file sys

    sim_link = kzalloc(32, GFP_KERNEL);
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    snprintf(sim_link, 32, "/proc/self/fd/%i", fd);
    ret = kernel_readlink(sim_link, fullpath_name, fullpath_size);
    kfree(sim_link);
    return ret;
}
