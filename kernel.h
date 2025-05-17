#pragma once

#include "common.h"

#define PAGE_SIZE 4096        // Size of a page
#define PROCS_MAX 8           // Maximum number of processes
#define PROC_UNUSED   0       // Unused process control structure
#define PROC_RUNNABLE 1       // Runnable process

#define USER_BASE 0x1000000   // load address of an user program.
#define MSTATUS_MPIE (1 << 7) // saved value of mstatus.MIE during an interrupt.
#define MCAUSE_ECALL 8        // mcause value for the ecall instruction from U-mode.
#define MCAUSE_TIMER 7        // mcause value for a timer interrupt.
#define PROC_EXITED  2

/// Process Control Block (PCB).
///
/// Each user process running is backed by a PCB.
///
/// Each PCB contains a stack that the kernel code uses when
/// it takes control of the CPU. A "thread" is really nothing
/// more than a stack and a register state, so each process
/// has an associated kernel thread. Any exception occuring
/// while a process is running will be handled by its associated
/// kernel thread.
struct process {
    int pid;               // Process ID
    int state;             // Process state: PROC_UNUSED or PROC_RUNNABLE.
    paddr_t sp;            // Stack pointer within the stack region.
    uint8_t stack[8192];   // Statically-allocated stack for the kernel.
};

// When a trap occurs, all registers used by user programs must
// be saved and restored before execution can continue. This
// structure stores the register state on the kernel stack and
// serves as input to the trap handler, allowing it to process
// arguments from the user program.
struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define panic(fmt, ...)                                                        \
    do {                                                                       \
        /* printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); */  \
        while (1) {}                                                           \
    } while (0)

#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp;                                                                 \
    })

#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint32_t __tmp = (value);                                              \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                \
    } while (0)
