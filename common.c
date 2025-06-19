#include "common.h"

extern void uart_putc(uint8_t c);

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = c;
    return buf;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Taken from https://github.com/gcc-mirror/gcc/blob/master/libgcc/config/epiphany/mulsi3.c
// TODO: panic if overflow
unsigned int __mulsi3(unsigned int a, unsigned int b)
{
    unsigned int r = 0;
    while (a)
    {
        if (a & 1)
            r += b;
        a >>= 1;
        b <<= 1;
    }
    return r;
}

void printf(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++; // Skip '%'
            switch (*fmt) { // Read the next character
                case '\0':
                    uart_putc('%');
                    goto end;
                case '%':
                    uart_putc('%');
                    break;
                case 's': { // Print a NULL-terminated string.
                    const char *s = va_arg(vargs, const char *);
                    while (*s) {
                        uart_putc(*s);
                        s++;
                    }
                    break;
                }
                case 'x': { // Print an integer in hexadecimal.
                    int value = va_arg(vargs, int);
                    for (int i = 7; i >= 0; i--) {
                        int nibble = (value >> (i * 4)) & 0xf;
                        uart_putc("0123456789abcdef"[nibble]);
                    }
                }
            }
        } else {
            uart_putc(*fmt);
        }

        fmt++;
    }

end:
    va_end(vargs);
}
