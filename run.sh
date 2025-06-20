#!/bin/bash
set -xue

# QEMU file path.
QEMU=qemu-system-riscv32

# Path to clang and compiler flags.
CC=clang
CFLAGS="-std=c11 -O2 -Wall -Wextra --target=riscv32 -march=rv32i -ffreestanding -nostdlib"

# Build the application.
$CC $CFLAGS -fPIE -Wl,-pie -Wl,-Tuser.ld -Wl,-Map=app.map -o app.elf app.c crt0.c

# Build the kernel.
$CC $CFLAGS -g3 -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf kernel.c common.c uart.c trap.S app.S

# Start QEMU
$QEMU -machine virt -bios none -nographic -serial mon:stdio -smp 1 --no-reboot -kernel kernel.elf
