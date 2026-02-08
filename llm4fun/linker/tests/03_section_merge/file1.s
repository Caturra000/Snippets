# 03_section_merge/file1.s

    .global _start
    .text

_start:
    call    func1
    mov     %rax, %rdi
    call    func2
    add     %rax, %rdi
    mov     $60, %rax
    syscall

func1:
    mov     $10, %rax
    ret
