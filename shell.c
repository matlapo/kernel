#include "crt0.h"

void main(void) {
    // *((volatile int *) 0x80200000) = 0x1234; illegal instruction
    for (;;);
}
