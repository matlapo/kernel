#include "common.h"

#define UART_BASE 0x10000000L
#define DLL 0x00
#define DLM 0x01
#define RHR 0x00
#define THR 0x00
#define IER 0x01
#define FCR 0x02
#define LCR 0x03
#define LSR 0x05

#define LSR_RX_READY    (1 << 0)
#define LSR_TX_IDLE     (1 << 5)

#define REG(offset) (*(volatile uint8_t *)(UART_BASE + offset))

void uart_init(void) {
    // Disable interrupts.
    REG(IER) = 0x00; 

    // Enable DLAB to set baud rate.
    REG(LCR) = 0x80;

    // Set baud rate to 38.4K.
    REG(DLL) = 0x03;
    REG(DLM) = 0x00;

    // Disable DLAB.
    // Set frame length to 8 bits, no parity, one stop bit.
    REG(LCR) = 0x03;

    // Enable FIFOs and clear both.
    REG(FCR) = 0x07;

    // Enable interrupts for when
    // (1) Data is available in RHR.
    // (2) THR is ready to accept a char.
    REG(IER) = 0x03;
}

void uart_putc(uint8_t c) {
    // Poll until THR is ready.
    while ((REG(LSR) & LSR_TX_IDLE) == 0)
        ;

    REG(THR) = c;
}
