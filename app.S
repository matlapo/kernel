.section .rodata.app, "a"
.global __app_start
.global __app_end

__app_start:
    .incbin "app.elf"
__app_end:
