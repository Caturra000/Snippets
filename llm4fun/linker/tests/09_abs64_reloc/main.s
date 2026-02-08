# 09_abs64_reloc/main.s

    .global _start
    .global my_func
    .text

_start:
    mov     func_ptr(%rip), %rax
    call    *%rax
    mov     %rax, %rdi
    mov     $60, %rax
    syscall

    .data
func_ptr:
    .quad   my_func

    .text
my_func:
    mov     $66, %rax
    ret
