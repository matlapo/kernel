#include "kernel.h"
#include "common.h"

extern char __kernel_base[];
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];

struct process procs[PROCS_MAX];

struct process *current_proc; // Currently running process
struct process *idle_proc;    // Idle process

// Prototypes
void yield(void);

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        // Whenever a user program is running, sscratch holds the
        // address of the associated kernel stack. That way, when
        // a trap occurs, the kernel can easily start using its
        // own stack to save the user's registers state and handle
        // the trap.
        //
        // Also save the user stack address in sscratch temporarily.
        "csrrw sp, sscratch, sp\n"

        // Save all general-purpose registers onto the kernel stack.
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // Save the user stack address on the kernel stack.
        "csrr a0, sscratch\n"
        "sw a0,  4 * 30(sp)\n"

        // Once the exception is handled and the user code
        // runs again, sscratch needs to hold the same value
        // it held prior to entering the kernel.
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        // handle_trap takes a pointer to a trap frame, which
        // is exactly what was just stored on the kernel stack.
        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

void handle_syscall(struct trap_frame *f) {
    switch (f->a3) {
        case SYS_PUTCHAR:
            panic("unimplemented");
            break;
        case SYS_GETCHAR:
            while (1) {
                panic("unimplemented");
                char ch = 0;
                if (ch >= 0) {
                    f->a0 = ch;
                    break;
                }

                yield();
            }
            break;
        case SYS_EXIT:
            // TODO: free up memory.
            // printf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            panic("unreachable");
        default:
            panic("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        // sepc points to the instruction that caused the exception.
        user_pc += 4;
    } else {
        panic("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

paddr_t alloc_pages(uint32_t n) {
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        panic("out of memory");

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

// Transfer control of the CPU from one kernel thread to another. When
// this function returns, it will be a different kernel thread than
// the one calling it.
__attribute__((naked))
void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
     // Unlike preemptive multitasking, where the preempted code is unaware
     // that it is losing control of the CPU, switching between kernel threads
     // is cooperative: it happens through a function call. When switch_context()
     // is invoked, the kernel explicitly switches from one kernel thread to
     // another. Since this is a function call rather than an exception, the
     // compiler automatically ensures that caller-saved registers (as per the
     // RISC-V calling convention) are saved on the stack beforehand. As a result,
     // switch_context() only needs to handle callee-saved registers. Since we
     // cannot predict which of these registers the next kernel thread will use,
     // the only good answer is to save them all.
    __asm__ __volatile__(
        // Save callee-saved registers onto the current process's stack.
        "addi sp, sp, -13 * 4\n"
        "sw ra,  0  * 4(sp)\n"
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n"
        "lw sp, (a1)\n"

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n"
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"
        "ret\n"
    );
}

// Creates a mapping between a virtual and physical page. The physical page must
// be allocated outside of this function.
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        panic("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        panic("unaligned paddr %x", paddr);

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        // Create the non-existent 2nd level page table.
        uint32_t pt_paddr = alloc_pages(1);
        // Extract the PPN from the physical address.
        table1[vpn1] = ((pt_paddr >> 12) << 10) | PAGE_V;
    }

    // Set the 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) << 12);
    table0[vpn0] = ((paddr >> 12) << 10) | flags | PAGE_V;
}

// Switches the CPU from S-mode to U-mode.
__attribute__((naked)) void user_entry(void) {
    // After sret executes, the CPU will jump to the
    // address stored in sepc.
    __asm__ __volatile__(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        "sret                      \n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE)
    );
}

// Sets up a new process.
//
// Takes as input a pointer to the user binary and maps its pages
// starting from the hardcoded virtual address USER_BASE. It is
// assumed that the program was linked (i.e. relocated) accordingly
// and that its first byte corresponds to the entry point of the binary.
//
// Panics if there is no free PCB available.
struct process *create_process(const void *image, size_t image_size) {
    // Find an unused process control structure.
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        panic("no free process slots");

    // When a kernel thread resumes execution, it is expected that
    // its previous register state was stored on its stack. Since
    // this kernel thread never ran before, each register should
    // be initialized to zero.
    //
    // As per the RISC-V calling convention, ra stores the return
    // address. Here, user_entry is used so that the CPU can be
    // set to U-mode and start executing the user program.
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;  // ra

    // Allocate a root page table.
    uint32_t *page_table = (uint32_t *) alloc_pages(1);

    // Map the kernel's memory 1:1
    for (paddr_t paddr = (paddr_t) __kernel_base; paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // Map user pages.
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        // Handle the case where the data to be copied is smaller than the page size.
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        // Copy a page of the image's bytes to the newly allocated page.
        // Copying is important otherwise two processes for the same application would share
        // the same physical pages, which would break isolation.
        memcpy((void *) page, image + off, copy_size);

        // Map the page (starting from USER_BASE) with user permissions.
        map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
    return proc;
}

void yield(void) {
    // Search for a runnable process
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    if (next == current_proc)
        return;

    // The sfence instruction guarantees completion of pending address translation
    // operations before continuing execution and flushes the TLB.
    //
    // Also store in sscratch the address of the bottom of the stack of the next
    // process to run.
    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    // Switch PCB
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

void kernel_main(void) {
    // Init the bss region with 0s.
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    // Set the exception handler.
    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    __asm__ __volatile__("addi x19, x0, 0x2");

    for (;;) {
        __asm__ __volatile__("wfi");
    }
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // Set the stack pointer
        "j kernel_main\n"       // Jump to the kernel main function
        :
        : [stack_top] "r" (__stack_top)
    );
}
