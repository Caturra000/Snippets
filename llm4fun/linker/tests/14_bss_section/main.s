# 14_bss_section/main.s

    .global _start
    .text

_start:
    mov     bss_var(%rip), %rdi
    test    %rdi, %rdi
    jnz     fail
    movq    $99, bss_var(%rip)
    mov     bss_var(%rip), %rdi
    mov     $60, %rax
    syscall

fail:
    mov     $1, %rdi
    mov     $60, %rax
    syscall

    .bss
    .align  8
bss_var:
    .skip   8
