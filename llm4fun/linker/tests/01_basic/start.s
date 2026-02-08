# 01_basic/start.s
# 最简单的程序：exit(42)

    .global _start
    .text

_start:
    mov     $60, %rax       # syscall: exit
    mov     $42, %rdi       # exit code
    syscall
