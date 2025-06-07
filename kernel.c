#include "kernel.h"
#include "common.h"

extern char __kernel_base[];
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];

extern void uart_putc(uint8_t c);

struct process procs[PROCS_MAX];

struct process *current_proc; // Currently running process
struct process *idle_proc;    // Idle process

void sched(void);

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        // Whenever a user program is running, mscratch holds the
        // address of the associated kernel stack. That way, when
        // a trap occurs, the kernel can easily start using its
        // own stack to save the user's registers state and handle
        // the trap.
        //
        // Also save the user stack address in mscratch temporarily.
        "csrrw sp, mscratch, sp\n"

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
        "csrr a0, mscratch\n"
        "sw a0,  4 * 30(sp)\n"

        // Once the exception is handled and the user code
        // runs again, mscratch needs to hold the same value
        // it held prior to entering the kernel.
        "addi a0, sp, 4 * 31\n"
        "csrw mscratch, a0\n"

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

        "mret\n"
    );
}

void handle_trap(struct trap_frame *f) {
    uint32_t mcause = READ_CSR(mcause);

//    if (mcause & (1 << 31) == 1) {
//        if (mcause & 0x7fffffff == 7) {
//           // TODO: handle timer interrupt.
//        }
//    } else {
//        // uint32_t uepc = READ_CSR(uepc);
//        if (mcause == MCAUSE_ECALL) {
//            // TODO: handle syscall.
//            // sepc points to the instruction that caused the exception.
//            mepc += 4;
//        } else {
//            panic("unexpected trap mcause=%x, mepc=%x\n", mcause, mepc);
//        }
//    }

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
    __asm__ __volatile__(
        "csrw mepc, %[mepc]        \n"
        "csrw mstatus, %[mstatus]  \n"
        "mret                      \n"
        :
        : [mepc] "r" (USER_BASE),
          [mstatus] "r" (MSTATUS_MPIE)
    );
}

void sched(void) {
    // Search for a runnable process
    struct process *next = idle_proc;
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

static inline uint64_t r_mtimecmp()
{
    return *(volatile uint64_t *)0x0204000;
}

void idle(void) {
    __asm__ __volatile__("addi x17, x0, 0x2");

    uint64_t x = r_mtimecmp();

    for (;;) {
        __asm__ __volatile__("wfi");
    }
}

void kernel_main(void) {
    // Init the bss region with 0s.
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    // Set the exception handler.
    WRITE_CSR(mtvec, (uint32_t) kernel_entry);

    struct process *kernel_proc = &procs[0];
    paddr_t *sp = (paddr_t *) &kernel_proc->stack[sizeof(kernel_proc->stack)];
    kernel_proc->pid = 0;
    kernel_proc->state = PROC_RUNNABLE;
    kernel_proc->sp = (paddr_t) sp;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = (uint32_t) idle;

    current_proc = &procs[0];

    uart_putc('H');
    uart_putc('e');
    uart_putc('l');
    uart_putc('l');
    uart_putc('o');
    uart_putc('\n');

    // Jump to idle for now, no timer interrupts so you'll be stuck there.
    idle();

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
