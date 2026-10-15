.globl handle_trap
.globl kernel_entry
.align 4
kernel_entry:
        /*
         * Interrupts are off during all of these instructions,
         * disabled by hardware before jumping here.
         *
         * Whenever a user program is running, mscratch holds the
         * address of the associated kernel stack, and 0 while the
         * kernel itself is running. So a trap from a user process
         * will push (and overwrite) data on the associated kernel
         * stack. A trap while executing kernel code will push a
         * frame on the same stack it was already using (i.e. won't
         * overwrite what's already on that stack).
         *
         * Swap sp with mscratch. mscratch now temporarily holds the
         * interrupted code's stack pointer.
         */
        csrrw sp, mscratch, sp

        /*
         * If mscratch was zero, then the trap came while executing kernel
         * code. So revert the sp change.
         */
        bnez sp, 1f
        csrr sp, mscratch
1:
        /* Save a trap frame onto the kernel stack. */
        addi sp, sp, -4 * 32
        sw ra,  4 * 0(sp)
        sw gp,  4 * 1(sp)
        sw tp,  4 * 2(sp)
        sw t0,  4 * 3(sp)
        sw t1,  4 * 4(sp)
        sw t2,  4 * 5(sp)
        sw t3,  4 * 6(sp)
        sw t4,  4 * 7(sp)
        sw t5,  4 * 8(sp)
        sw t6,  4 * 9(sp)
        sw a0,  4 * 10(sp)
        sw a1,  4 * 11(sp)
        sw a2,  4 * 12(sp)
        sw a3,  4 * 13(sp)
        sw a4,  4 * 14(sp)
        sw a5,  4 * 15(sp)
        sw a6,  4 * 16(sp)
        sw a7,  4 * 17(sp)
        sw s0,  4 * 18(sp)
        sw s1,  4 * 19(sp)
        sw s2,  4 * 20(sp)
        sw s3,  4 * 21(sp)
        sw s4,  4 * 22(sp)
        sw s5,  4 * 23(sp)
        sw s6,  4 * 24(sp)
        sw s7,  4 * 25(sp)
        sw s8,  4 * 26(sp)
        sw s9,  4 * 27(sp)
        sw s10, 4 * 28(sp)
        sw s11, 4 * 29(sp)

        /* Also save the interrupted code's stack pointer. */
        csrr a0, mscratch
        sw a0,  4 * 30(sp)

        /*
         * Once we return from handle_trap, we need a way to remember
         * that if we came here from user or kernel code to set mscratch
         * accordingly.
         *
         * a0 stores the interrupted code's sp.
         * a1 stores the top of the kernel stack.
         * If a0 == a1, then the trap came from kernel code.
         */
        addi a1, sp, 4 * 32
        bne a0, a1, 2f
        mv a1, zero
2:
        sw a1, 4 * 31(sp)

        /* We're running kernel code, so mscratch must be zero. */
        csrw mscratch, zero

        /*
         * handle_trap takes a pointer to a trap frame struct, which
         * is exactly what was just stored on the stack here.
         */
        mv a0, sp
        call handle_trap

        /* Restore mscratch first since we need to use temporary space. */
        lw a0, 4 * 31(sp)
        csrw mscratch, a0

        lw ra,  4 * 0(sp)
        lw gp,  4 * 1(sp)
        lw tp,  4 * 2(sp)
        lw t0,  4 * 3(sp)
        lw t1,  4 * 4(sp)
        lw t2,  4 * 5(sp)
        lw t3,  4 * 6(sp)
        lw t4,  4 * 7(sp)
        lw t5,  4 * 8(sp)
        lw t6,  4 * 9(sp)
        lw a0,  4 * 10(sp)
        lw a1,  4 * 11(sp)
        lw a2,  4 * 12(sp)
        lw a3,  4 * 13(sp)
        lw a4,  4 * 14(sp)
        lw a5,  4 * 15(sp)
        lw a6,  4 * 16(sp)
        lw a7,  4 * 17(sp)
        lw s0,  4 * 18(sp)
        lw s1,  4 * 19(sp)
        lw s2,  4 * 20(sp)
        lw s3,  4 * 21(sp)
        lw s4,  4 * 22(sp)
        lw s5,  4 * 23(sp)
        lw s6,  4 * 24(sp)
        lw s7,  4 * 25(sp)
        lw s8,  4 * 26(sp)
        lw s9,  4 * 27(sp)
        lw s10, 4 * 28(sp)
        lw s11, 4 * 29(sp)
        lw sp,  4 * 30(sp)

        mret
