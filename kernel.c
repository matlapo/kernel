#include "kernel.h"
#include "common.h"
#include "riscv.h"

extern char __kernel_base[];
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];
extern char __app_start[], __app_end[];

extern void uart_putc(uint8_t c);
extern void kernel_entry(void);

// The kernel maintains a list of processes. Each process has a
// statically-allocated stack. This whole structure is allocated
// in the .bss section by clang and zeroed at boot time by
// kernel_main.
struct process procs[PROCS_MAX];

struct process *current_proc; // Currently running process

void sched(void);
void set_mtimecmp();

void handle_trap(struct trap_frame *f) {
    uint32_t mcause = READ_CSR(mcause);

    printf("Trap\n");

    uint32_t exception_code = mcause & MCAUSE_CODE;
    if ((mcause >> 31) == 1) {
        if (exception_code == MCAUSE_TIMER) {
            set_mtimecmp();
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
// the one calling it (different stack).
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
        "addi sp, sp, -16 * 4\n"
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
        "addi sp, sp, 16 * 4\n"

        "ret\n"
    );
}

// Switches the CPU from M-mode to U-mode.
__attribute__((naked))
void user_entry(void) {
    // After mret executes, the CPU will jump to the
    // address stored in mepc.
    // TODO: fix up mepc.
    // TODO: need to set mscratch to the top of the kernel stack
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
    for (int i = 1; i <= PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE) {
            next = proc;
            break;
        }
    }

    if (next == current_proc)
        return;

    // Switch PCB
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

struct elf_header {
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf_ph {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

static uint16_t read_u16(void **p) {
    uint8_t *d = *(uint8_t **)p;
    uint16_t v = d[0] | (d[1] << 8);
    *p += 2;
    return v;
}

static uint32_t read_u32(void **p) {
    uint8_t *d = *(uint8_t **)p;
    uint32_t v = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
    *p += 4;
    return v;
}

__attribute__((noreturn))
void load_app(void) {
    const paddr_t load_address = (paddr_t) __app_start;
    uint8_t *elf_p = (uint8_t *) __app_start;
    printf("elf_p is %x\n", elf_p);

    const uint8_t elf_magic[16] = {
        0x7F, 0x45, 0x4C, 0x46,
        0x01, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    if (memcmp(elf_p, elf_magic, 16) != 0) {
        printf("Not an ELF file\n");
        // exit();
    }
    elf_p += 16;

    uint16_t e_type = read_u16((void **)&elf_p);
    uint16_t e_machine = read_u16((void **)&elf_p);
    uint32_t e_version = read_u32((void **)&elf_p);
    uint32_t e_entry = read_u32((void **)&elf_p);
    uint32_t e_phoff = read_u32((void **)&elf_p);
    uint32_t e_shoff = read_u32((void **)&elf_p);
    uint32_t e_flags = read_u32((void **)&elf_p);
    uint16_t e_ehsize = read_u16((void **)&elf_p);
    uint16_t e_phentsize = read_u16((void **)&elf_p);
    uint16_t e_phnum = read_u16((void **)&elf_p);
    uint16_t e_shentsize = read_u16((void **)&elf_p);
    uint16_t e_shnum = read_u16((void **)&elf_p);
    uint16_t e_shstrndx = read_u16((void **)&elf_p);

    struct elf_header header = {
        .e_type = e_type,
        .e_machine = e_machine,
        .e_version = e_version,
        .e_entry = e_entry,
        .e_phoff = e_phoff,
        .e_shoff = e_shoff,
        .e_flags = e_flags,
        .e_ehsize = e_ehsize,
        .e_phentsize = e_phentsize,
        .e_phnum = e_phnum,
        .e_shentsize = e_shentsize,
        .e_shnum = e_shnum,
        .e_shstrndx = e_shstrndx,
    };

    elf_p = (uint8_t *) load_address + header.e_phoff;
    uint32_t p_type = read_u32((void **)&elf_p);
    uint32_t p_offset = read_u32((void **)&elf_p);
    uint32_t p_vaddr = read_u32((void **)&elf_p);
    uint32_t p_paddr = read_u32((void **)&elf_p);
    uint32_t p_filesz = read_u32((void **)&elf_p);
    uint32_t p_memsz = read_u32((void **)&elf_p);
    uint32_t p_flags = read_u32((void **)&elf_p);
    uint32_t p_align = read_u32((void **)&elf_p);

    struct elf_ph ph_1 = {
        .p_type = p_type,
        .p_offset = p_offset,
        .p_vaddr = p_vaddr,
        .p_paddr = p_paddr,
        .p_filesz = p_filesz,
        .p_memsz = p_memsz,
        .p_flags = p_flags,
        .p_align = p_align,
    };

    printf("ptype = %x\n", p_type);
    printf("poffset = %x\n", p_offset);
    printf("pvaddr = %x\n", p_vaddr);
    printf("ppaddr = %x\n", p_paddr);
    printf("pfilesz = %x\n", p_filesz);
    printf("pmemsz = %x\n", p_memsz);
    printf("pflags = %x\n", p_flags);
    printf("palign = %x\n", p_align);

    struct process *app_proc = &procs[2];
    paddr_t *sp = (paddr_t *) &app_proc->stack[sizeof(app_proc->stack)];

    app_proc->pid = 2;
    app_proc->state = PROC_RUNNABLE;

    // switch_context() assumes the next thread's register state is already saved
    // on its stack.
    for (int i = 0; i < SWITCH_FRAME_WORDS - 1; i++) {
        *--sp = 0;
    }
    *--sp = (paddr_t) load_address + header.e_entry;

    app_proc->sp = (paddr_t) sp;

    // TODO: should really call exit() here and the trap handler will
    // call sched().
    // sched();

    for (int i = 0; i < 10; i++) {
        printf("elf_p points to byte %x\n", *elf_p);
        elf_p++;
    }

    for (;;) {
        printf("Hella!\n");
        __asm__ __volatile__("wfi");
    }

    // Create process for app.
    // Exit().
}

void init(void) {
    // Enable interrupts.
    WRITE_CSR(mstatus, READ_CSR(mstatus) | MSTATUS_MIE);

    // ELF loader.
    struct process *loader_proc = &procs[1];
    paddr_t *sp = (paddr_t *) &loader_proc->stack[sizeof(loader_proc->stack)];

    loader_proc->pid = 1;
    loader_proc->state = PROC_RUNNABLE;

    // switch_context() assumes the next thread's register state is already saved
    // on its stack.
    for (int i = 0; i < SWITCH_FRAME_WORDS - 1; i++) {
        *--sp = 0;
    }
    *--sp = (paddr_t) load_app;

    loader_proc->sp = (paddr_t) sp;

    sched();

    for (;;) {
        printf("Hello!\n");
        __asm__ __volatile__("wfi");
    }
}

__attribute__((noreturn))
void switch_to_init(paddr_t sp) {
    __asm__ __volatile__ (
        "mv sp, %[sp]\n"
        "csrw mscratch, zero\n"
        "call init\n"
        :
        : [sp] "r" (sp)
    );
}

void set_mtimecmp(void) {
    uint64_t mtime = r_mtime();
    uint64_t t = mtime + TIMER_TICKS;
    w_mtimecmp(t);
}

void kernel_main(void) {
    // Init the bss region with 0s.
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    // Set the trap handler.
    WRITE_CSR(mtvec, (uint32_t) kernel_entry);

    // Enable timer interrupts.
    set_mtimecmp();
    WRITE_CSR(mie, READ_CSR(mie) | MIE_TIMER);

    struct process *kernel_proc = &procs[0];
    paddr_t *sp = (paddr_t *) &kernel_proc->stack[sizeof(kernel_proc->stack)];

    kernel_proc->pid = 0;
    kernel_proc->state = PROC_RUNNABLE;
    kernel_proc->sp = (paddr_t) sp;

    current_proc = &procs[0];

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
