# 12_dup_error/file1.s

    .global _start
    .global duplicate_sym
    .text

_start:
    call    duplicate_sym
    mov     $60, %rax
    syscall

duplicate_sym:
    mov     $1, %rax
    ret
