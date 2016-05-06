#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include "common.h"
extern asmlinkage long (*sys_readlink)(const char __user *path, char __user *buf, int bufsiz);
extern asmlinkage long (*sys_getcwd)(char __user *buf, unsigned long size);

long kernel_readlink(const char __user *path, char __user *buf, int bufsize);
long kernel_getcwd(char __user *buf, unsigned long size);
int fd_2_fullpath(unsigned int fd, char *fullpath_name, int fullpath_size);
#endif
