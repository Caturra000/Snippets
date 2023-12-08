.text
   .global _start
_start:
   movq $100000000, %rcx
TEST:
   pushq %rcx
   movq $4, %rax    # sys_write
   movq $-1, %rbx
   movq $0, %rcx
   movq $0, %rdx
   int $0x80
   popq %rcx
   loop TEST
   movq $1, %rax    # sys_exit
   xorq %rbx, %rbx
   int $0x80
