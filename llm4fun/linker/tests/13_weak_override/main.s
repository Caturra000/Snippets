# 13_weak_override/main.s

    .global _start
    .text

_start:
    call    get_number
    mov     %rax, %rdi
    mov     $60, %rax
    syscall
