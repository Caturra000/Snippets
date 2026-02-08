# 02_multi_obj/main.s

    .global _start
    .text

_start:
    call    get_exit_code
    mov     %rax, %rdi
    mov     $60, %rax
    syscall
