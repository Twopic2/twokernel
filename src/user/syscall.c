#include "syscall.h"

long getpid(void) {
    return __syscall0(SYS_GETPID);
}

void exit(int status) {
    __syscall1(SYS_EXIT, status);
}
