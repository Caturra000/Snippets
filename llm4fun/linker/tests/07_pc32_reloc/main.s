# 07_pc32_reloc/main.s

    .global _start
    .text

_start:
    call    target
    mov     %rax, %rdi
    mov     $60, %rax
    syscall
