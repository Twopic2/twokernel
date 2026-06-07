# TwoKernel

A hobbyist monolithic kernel written in c++.

# Resources

Limine-c++-template:
https://codeberg.org/Limine/limine-cxx-template/src/branch/trunk/limine.conf

# Roadmap

## Completed

### 01 — Boot & C++ runtime
- [x] Limine config + base revision check
- [x] C++ global constructor invocation
- [x] `hcf` halt routine
- [x] Linker script

### 02 — Output & logging
- [x] Framebuffer driver
- [x] Flanterm-backed TTY
- [x] `klog` printf-style kernel logger
- [x] Minimal libc (memset/memcpy/strlen, etc.)

### 03 — CPU descriptors & interrupts
- [x] GDT load
- [x] IDT + ISR stubs
- [x] Exception handlers
- [x] PIC remap + IRQ mask scaffolding

### 04 — Memory
- [x] PMM with free-list allocator
- [x] VMM: 4-level paging, kernel HHDM mapping, kernel section mapping with NX, EFER.NX, CR3 load
- [x] Reclaim `bootloader_reclaimable` memory after VMM init (stub exists, not wired into `kmain`)

## 05 — Scheduling
- [x] `Thread`/`Task` struct (registers, kernel stack, state)
- [x] Context switch in asm (save/restore GPRs, RFLAGS, RIP, RSP)
- [x] Ready queue + round-robin scheduler
- [x] Timer-driven preemption (PIC timer ISR → `schedule()`)
- [?] `sleep`/wait queues, blocking primitives
- [ ] Idle thread

## 06 — Userspace
- [ ] User CS/DS/TSS in GDT, IST stacks
- [ ] Per-process address space (clone kernel-half PML4 entries into new pagemap)
- [ ] User page mapping flags (USER bit) in VMM
- [ ] ELF64 loader (program headers → mmap into address space)
- [ ] `iretq`-based ring 3 entry
- [ ] Syscall entry: SYSCALL/SYSRET, MSR setup (STAR/LSTAR/SFMASK)
- [ ] Initial syscall set (write, exit, getpid, mmap, brk)
- [ ] Kernel heap (`kmalloc`/`kfree`, slab or bump-then-free-list on top of PMM)
- [ ] `unmap` should also free intermediate page tables when empty + INVLPG / TLB shootdown
- [ ] Replace PIC with APIC + LAPIC timer (or HPET) — needed for preemptive scheduling
- [ ] ACPI table parsing (RSDP/MADT) — feeds APIC/SMP
- [ ] PS/2 keyboard or serial input for interactive debugging

## 07 — IPC
- [ ] Pipes or message ports
- [ ] Shared memory regions (VMM API for shared mappings)
- [ ] Signals or equivalent async notification
- [ ] Synchronization primitives exposed to userspace (futex-like)

## 08 — Virtual File System
- [ ] VFS node abstraction (`vnode`/`inode`, ops table)
- [ ] Mountpoint + path resolution
- [ ] In-memory `tmpfs` as first backend
- [ ] File descriptor table per process
- [ ] `open`/`read`/`write`/`close`/`stat` syscalls
- [ ] A real disk FS (FAT32 or ext2) and a block driver (AHCI/virtio-blk) when ready

