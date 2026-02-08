# 05_weak_symbol/main.s

    .global _start
    .text

_start:
    call    get_value
    mov     %rax, %rdi
    mov     $60, %rax
    syscall
