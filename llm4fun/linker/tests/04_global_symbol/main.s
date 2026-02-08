# 04_global_symbol/main.s

    .global _start
    .text

_start:
    mov     global_var(%rip), %rdi
    mov     $60, %rax
    syscall
