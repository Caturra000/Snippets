# 06_local_symbol/main.s

    .global _start
    .text

_start:
    call    helper
    mov     %rax, %rdi
    mov     $60, %rax
    syscall

helper:
    mov     $11, %rax
    ret
