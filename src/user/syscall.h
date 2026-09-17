#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include "arch/x86-64/arch/sysv_syscalls.h"

/*
    ? Userspace side of the syscall table.
    ? Numbers follow the Linux x86_64 table, same slots as Syscalls::init_syscalls() in the kernel.
*/
#define SYS_GETPID 39
#define SYS_EXIT   60

long getpid(void);
void exit(int status);

#endif
