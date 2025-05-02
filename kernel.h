#pragma once

#include "common.h"

#define PAGE_SIZE 4096       // Size of a page
#define PROCS_MAX 8          // Maximum number of processes
#define PROC_UNUSED   0      // Unused process control structure
#define PROC_RUNNABLE 1      // Runnable process
#define SATP_SV32 (1u << 31) // Enable paging bit
#define PAGE_V    (1 << 0)   // Valid bit (entry is enabled)
#define PAGE_R    (1 << 1)   // Readable
#define PAGE_W    (1 << 2)   // Writable
#define PAGE_X    (1 << 3)   // Executable
#define PAGE_U    (1 << 4)   // User (accessible in user mode)

// The base virtual address of an application image.
// A user program needs to be linked/relocated starting from
// this memory address.
#define USER_BASE 0x1000000

#define SSTATUS_SPIE (1 << 5) // Enable U-mode for the CPU.

#define SCAUSE_ECALL 8 // sscause value for the ecall instruction.

#define PROC_EXITED   2

/// Process Control Block (PCB).
///
/// Each user process running is backed by a PCB.
///
/// Each PCB contains a stack that the kernel code uses when
/// it takes control of the CPU. Since the concept of a "thread"
/// is really nothing more than a stack and a register state,
/// this basically means that each user process has an associated
/// kernel thread. Any exception occuring while user code is
/// running is handled by that associated kernel thread.
///
/// The kernel is essentially a collection of pages containing
/// data and instructions meant to be executed in S-mode. There
/// is not "a" kernel, rather, there are kernel _pages_ that can
/// be made reachable by mapping them in a page table. Any thread
/// that can access and execute code on these pages is part of
/// "the kernel".
///
/// Each PCB has its own page table. The resulting virtual address
/// space is shared between the kernel and the user program. The
/// kernel instructions and data are mapped using identity mapping,
/// ensuring that kernel code runs at the same physical addresses
/// across all processes. That means every kernel thread share the
/// same physical memory despite each using a different page table.
struct process {
    int pid;               // Process ID
    int state;             // Process state: PROC_UNUSED or PROC_RUNNABLE.
    vaddr_t sp;            // Stack pointer within the stack region.
    uint32_t *page_table;  // Page table address, shared between kernel and user.
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
