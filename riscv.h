#pragma once

#define TIMER_TICKS 10000000    // Number of ticks before the next timer interrupt.
#define MSTATUS_MIE (1 << 3)    // Interrupts enabled.
#define MIE_TIMER (1 << 7)      // Timer interrupts enabled.
#define MSTATUS_MPIE (1 << 7)   // Saved value of mstatus.MIE during an interrupt.
#define MCAUSE_CODE 0x7FFFFFFF  // Exception cause
#define MCAUSE_ECALL 8          // mcause value for the ecall instruction from U-mode.
#define MCAUSE_TIMER 7          // mcause value for a timer interrupt.
#define MMIO_MTIMECMP 0x2004000 // Address of mtimecmp for hart 0.
#define MMIO_MTIME 0x0200BFF8   // Address of mtime.

static inline uint64_t r_mtimecmp()
{
    return *(volatile uint64_t *)MMIO_MTIMECMP;
}

static inline void w_mtimecmp(uint64_t mtimecmp)
{
    *(volatile uint64_t *)MMIO_MTIMECMP = mtimecmp;
}

static inline uint64_t r_mtime()
{
    uint32_t hi1, lo, hi2;
    volatile uint32_t *mtime32 = (uint32_t *)MMIO_MTIME;

    // On 32-bit RISC-V, a C compiler turns a 64-bit read as
    // two 32-bit loads. If we're unlucky and mtime increments
    // between the low and high loads (e.g., when the low part
    // wraps around), it would make it seem like mtime has
    // decremented.
    do {
        hi1 = mtime32[1];
        lo = mtime32[0];
        hi2 = mtime32[1];
    } while (hi1 != hi2);

    uint64_t mtime = ((uint64_t)hi2 << 32) | lo;
    return mtime;
}
