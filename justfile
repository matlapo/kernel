cc := env("CC", if os() == "macos" { "/opt/homebrew/opt/llvm/bin/clang" } else { "clang" })
ld := if os() == "macos" { "-fuse-ld=lld" } else { "" }
cflags := "-std=c11 -O2 -Wall -Wextra --target=riscv32 -march=rv32i -ffreestanding -nostdlib " + ld

# app.elf must be built first: app.S embeds it into the kernel.
build:
    {{cc}} {{cflags}} -fPIE -Wl,-pie -Wl,-Tuser.ld -Wl,-Map=app.map -o app.elf app.c crt0.c
    {{cc}} {{cflags}} -g3 -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf kernel.c common.c uart.c trap.S app.S

run: build
    qemu-system-riscv32 -machine virt -bios none -nographic -serial mon:stdio -smp 1 --no-reboot -kernel kernel.elf
