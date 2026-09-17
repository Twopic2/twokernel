/*
#include <cstdint>
*/

/*
    ! Notes
    ? So the name of the files is a bit confusing due to there being vmem/vmm, ignore that for a sec.

    ? Once we have some kind of address translation enabled, all memory we can access is now virtual memory. 
    ? This address translation is usually performed by the MMU (memory management unit) which we can program in someway.

    ? The old way we used to translate pa -> va vise verse within kernel space was through the hhdm offset. But since 
    ? we are on usermode there is no easy transition. Our vmem is basically our allocator for userspace 


    ? VmObject 
    * address space:  [--obj A--]      [----obj B----]          [-obj C-]
    *                            ^^^^^^^^             ^^^^^^^^^^
    *                             free gap             free gap   (implicit — no nodes)
*/

/* 
namespace Memory::Vmem {
    inline constexpr auto USER_SPACE_END = 0x7FFFFFFFF000;

    struct VmObject {
        PageMap* base;
        std::uint64_t length;
        std::uint64_t flags;
        VmObject* next;
    };

    void init_alloc();
}
 */
 