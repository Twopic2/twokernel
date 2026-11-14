#include "syscall.h"

int main(void);

void _start(void) {
    exit(main());

    /* Stay here if the exit syscall returns. */
    for (;;) {}
}

int main(void) {
    return 0;
}
