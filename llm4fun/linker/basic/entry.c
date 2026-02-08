extern int compute(int x);

void _start(void) {
    int result = compute(39);     // 39 + 3 = 42
    asm volatile(
        "movl %0, %%edi\n\t"      // exit code = result
        "movl $60, %%eax\n\t"     // sys_exit
        "syscall"
        :: "r"(result) : "rdi", "rax"
    );
    __builtin_unreachable();
}