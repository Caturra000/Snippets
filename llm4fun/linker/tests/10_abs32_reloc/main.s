# 10_abs32_reloc/main.s

    .global _start
    .text

_start:
    lea     value(%rip), %rax
    mov     (%rax), %rdi
    mov     $60, %rax
    syscall

    .data
value:
    .quad   88
