# 11_undef_error/main.s

    .global _start
    .text

_start:
    call    undefined_function
    mov     $60, %rax
    syscall
