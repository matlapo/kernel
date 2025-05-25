#!/bin/bash
set -xue

# QEMU file path
QEMU=qemu-system-riscv32

OBJCOPY=/usr/bin/llvm-objcopy

# Path to clang and compiler flags
CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32 -march=rv32i -ffreestanding -nostdlib"

# Build the kernel
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf kernel.c common.c uart.c

# Start QEMU
$QEMU -machine virt -bios none -nographic -serial mon:stdio --no-reboot -kernel kernel.elf
