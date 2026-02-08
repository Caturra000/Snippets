# 08_plt32_reloc/main.s

    .global _start
    .text

_start:
    call    external_func@PLT
    mov     %rax, %rdi
    mov     $60, %rax
    syscall
