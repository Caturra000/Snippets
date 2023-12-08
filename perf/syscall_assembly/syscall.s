.text
   .global _start
_start:
   movq $100000000, %rcx
TEST:
   pushq %rcx
   movq $1, %rax    # sys_write
   movq $-1, %rdi   # Invalid fd.
   movq $0, %rsi    # Empty string.
   movq $0, %rdx    # Empty length.
   syscall
   popq %rcx
   loop TEST
   movq $60, %rax   # sys_exit
   xorq %rdi, %rdi
   syscall
