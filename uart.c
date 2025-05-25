#include "common.h"

#define UART_BASE 0x10000000
#define RBR 0x00 // Receiver buffer register
#define THR 0x00 // Transmitter holder register
#define IER 0x04 // Interrupt enable register
#define FCR 0x08 // FIFO control register
#define LCR 0x0c // Line control register
#define LSR 0x14 // Line status register

#define REG(reg) ((volatile uint8_t *)(UART_BASE + reg))

void uart_init(void) {
    volatile uint8_t *rbr = REG(RBR);
}
