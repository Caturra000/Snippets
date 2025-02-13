.section .text
.global _start

_start:
    # 初始值设为 0xffffffffffffffff
    movq $-1, %rax

    movb $0, %al
    # (gdb) info registers rax 输出 0xffffffffffffff00

    movw $1, %ax
    # (gdb) info registers rax 输出 0xffffffffffff0001

    movl $2, %eax
    # (gdb) info registers rax 输出 0x2（截断）

    # 退出程序，syscall=exit
    movl $60, %eax
    xorl %edi, %edi
    syscall
