#include "kernel.h"
#include "common.h"
#include "riscv.h"

extern char __kernel_base[];
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];

extern void uart_putc(uint8_t c);
extern void kernel_entry(void);

struct process procs[PROCS_MAX];

struct process *current_proc; // Currently running process

void sched(void);

void handle_trap(struct trap_frame *f) {
    uint32_t mcause = READ_CSR(mcause);

    printf("Trap\n");

    uint32_t exception_code = mcause & MCAUSE_CODE;
    if ((mcause >> 31) == 1) {
        if (exception_code == MCAUSE_TIMER) {
            uint64_t mtimecmp = r_mtimecmp();
            uint64_t mtime = r_mtime();

            uint64_t t = mtime + TIMER_TICKS;
            w_mtimecmp(t);
        }
    } else {
        printf("Synchronous exception\n");
        printf("mcause is %x\n", mcause);
        uint32_t mepc = READ_CSR(mepc);
        printf("mepc is %x\n", mepc);
//        // uint32_t uepc = READ_CSR(uepc);
//        if (mcause == MCAUSE_ECALL) {
//            // TODO: handle syscall.
//            // sepc points to the instruction that caused the exception.
//            mepc += 4;
//        } else {
//            panic("unexpected trap mcause=%x, mepc=%x\n", mcause, mepc);
//        }
    }

// TODO: prologue and epilogue should be the same regardless of the
// exception kind.
//    WRITE_CSR(mepc, mepc);
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
     // Just like preemptive multitasking, every register needs to be saved somewhere
     // before switching to a different stack. But unlike preemptive multitasking,
     // where the preempted code is unaware that it is losing control of the CPU,
     // switching between kernel threads is cooperative: it happens through a
     // function call. When switch_context() is invoked, the kernel explicitly
     // switches from one kernel thread to another. Since this is a function call
     // rather than an exception, the compiler helps us by automatically saving the
     // caller-saved registers (as per the RISC-V calling convention) on the caller's
     // stack before entering the function. As a result, switch_context() only needs
     // to handle callee-saved registers.
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

// Switches the CPU from M-mode to U-mode.
__attribute__((naked))
void user_entry(void) {
    // After mret executes, the CPU will jump to the
    // address stored in mepc.
    // TODO: fix up mepc.
    __asm__ __volatile__(
        "csrw mepc, %[mepc]        \n"
        "csrw mstatus, %[mstatus]  \n"
        "mret                      \n"
        :
        : [mepc] "r" (0),
          [mstatus] "r" (MSTATUS_MPIE)
    );
}

void sched(void) {
    // Search for a runnable process
    struct process *next = &procs[0];
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE) {
            next = proc;
            break;
        }
    }

    if (next == current_proc)
        return;

    // Exception handlers expect mscratch to hold the kernel stack address of
    // the currently running process.
    __asm__ __volatile__(
        "csrw mscratch, %[mscratch]\n"
        :
        : [mscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    // Switch PCB
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

void init(void) {
    printf("Hello!\n");

    // Enable interrupts.
    WRITE_CSR(mstatus, READ_CSR(mstatus) | MSTATUS_MIE);

    for (;;) {
        printf("Hello!\n");
        __asm__ __volatile__("wfi");
    }
}

__attribute__((noreturn))
void switch_to_init(paddr_t sp) {
    __asm__ __volatile__ (
        "mv sp, %[sp]\n"
        "csrw mscratch, %[sp]\n"
        "call init\n"
        :
        : [sp] "r" (sp)
    );
}

void kernel_main(void) {
    // Init the bss region with 0s.
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    // Set the exception handler.
    WRITE_CSR(mtvec, (uint32_t) kernel_entry);

    // Enable timer interrupts.
    WRITE_CSR(mie, READ_CSR(mie) | MIE_TIMER);

    struct process *kernel_proc = &procs[0];
    paddr_t *sp = (paddr_t *) &kernel_proc->stack[sizeof(kernel_proc->stack)];

    // The RISC-V calling convention specifies that the stack must be 16-byte
    // aligned at call boundaries.
    sp = (paddr_t *) align_up(sp, 16);

    kernel_proc->pid = 0;
    kernel_proc->state = PROC_RUNNABLE;
    kernel_proc->sp = (paddr_t) sp;

    current_proc = &procs[0];

    uint32_t *elf_p = (uint32_t *) 0x000048bd; // __free_ram_end;
    printf("elf_p is %x\n", elf_p);
    for(int i = 0; i < 100; i++) {
        elf_p++;
        printf("elf_p points to byte %x\n", *elf_p);
    }

    // Start executing using PID 0's stack, never return.
    switch_to_init((paddr_t)sp);

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
