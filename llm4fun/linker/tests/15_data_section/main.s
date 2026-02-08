# 15_data_section/main.s

    .global _start
    .text

_start:
    mov     data_var(%rip), %rax
    cmp     $123, %rax
    jne     fail
    movq    $77, data_var(%rip)
    mov     data_var(%rip), %rdi
    mov     $60, %rax
    syscall

fail:
    mov     $1, %rdi
    mov     $60, %rax
    syscall

    .data
    .align  8
data_var:
    .quad   123
