#include "arch/x86-64/arch/syscall.hpp"
#include "util/kernel_logger.hpp"
#include "util/ansi.hpp"
#include <cstdint>

namespace Syscalls {
    /*
        ? Standing in for per-CPU state. syscall_hanlder has no gs base to work
        ? from, so the kernel stack it switches to comes from here -- written
        ? alongside TSS.rsp0 on every context switch. Becomes %gs:offset with
        ? swapgs once SMP arrives.
    */
    extern "C" {
        std::uint64_t syscall_kernel_rsp {};
        std::uint64_t syscall_user_rsp {};
    }

    /* Linux numbering, so binaries built against a real libc work unchanged. */
    inline constexpr std::uint64_t SYS_GETPID = 39;

    inline constexpr std::int64_t ENOSYS = -38;

    extern "C" void syscall_dispatch(x86::ArchIrq::IrqFrame* regs) {
        SyscallFrame frame { *regs };

        Util::klog("%s%s[syscall] num=%llu arg0=0x%llx from_user=%u%s\n",
                   BOLD, BLUE,
                   frame.number(), frame.arg0(),
                   static_cast<unsigned>(frame.from_user()),
                   RESET);

        switch (frame.number()) {
            case SYS_GETPID:
                frame.set_return(0);
                break;

            default:
                Util::klog("%s[syscall] unimplemented %llu -> ENOSYS%s\n",
                           RED, frame.number(), RESET);
                frame.set_error(ENOSYS);
                break;
        }
    }
}
