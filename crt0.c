#include "crt0.h"

extern char __stack_top[];

__attribute__((noreturn)) void exit(void) {
    for (;;);
}

void putchar(char ch) {
    /* TODO */
}

// Entry point for a user program.
__attribute__((section(".text.start")))
__attribute__((naked))
void start(void) {
    // A 64Kb stack is statically allocated via the
    // user linker script. Before jumping to main,
    // point sp to it.
    __asm__ __volatile__(
        "mv sp, %[stack_top] \n"
        "call main           \n"
        "call exit           \n"
        :: [stack_top] "r" (__stack_top)
    );
}
