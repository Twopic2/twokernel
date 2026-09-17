#ifndef POSIX_SYSCALLS_H
#define POSIX_SYSCALLS_H

// Functions preserve the registers rbx, rsp, rbp, r12, r13, r14, and r15;
// while rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11 are scratch registers.

static inline long __syscall0(long num) {
	long ret;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");

	return ret;
}

static inline long __syscall1(long num, long a0) {
	long ret;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(a0) : "rcx", "r11", "memory");

	return ret;
}

static inline long __syscall2(long num, long a0, long a1) {
	long ret;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1) : "rcx", "r11", "memory");

	return ret;
}

static inline long __syscall3(long num, long a0, long a1, long a2) {
	long ret;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1), "d"(a2) : "rcx", "r11", "memory");

	return ret;
}

static inline long __syscall4(long num, long a0, long a1, long a2, long a3) {
	long ret;
	register long r10 __asm__("r10") = a3;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1), "d"(a2), "r"(r10) : "rcx", "r11", "memory");

	return ret;
}

static inline long __syscall5(long num, long a0, long a1, long a2, long a3, long a4) {
	long ret;
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8") = a4;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8) : "rcx", "r11", "memory");

	return ret;
}

static inline long __syscall6(long num, long a0, long a1, long a2, long a3, long a4, long a5) {
	long ret;
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5;
	__asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");

	return ret;
}

#endif
