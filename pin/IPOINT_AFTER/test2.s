# To compile:
# gcc test2.s -o test2 -nostdlib -static

.section .text
.globl _start

_start:
    # 1. 普通指令 (非分支)
    movq $0, %rax               # 预期: 只触发 AFTER

check_not_taken:
    # 2. 条件分支 (不跳转)
    cmpq $1, %rax               # 预期: 只触发 AFTER
    je  branch_was_taken        # rax is 0, so this jump is NOT taken
                                # 预期: 只触发 AFTER

    # 3. 又一个普通指令
    incq %rax                   # rax is now 1. 预期: 只触发 AFTER

check_taken:
    # 4. 条件分支 (会跳转)
    cmpq $1, %rax               # 预期: 只触发 AFTER
    je  branch_was_taken        # rax is 1, so this jump IS taken
                                # 预期: 只触发 TAKEN_BRANCH

    # 这条指令永远不会被执行
    movq $999, %rcx

branch_was_taken:
    # 5. 无条件分支 (会跳转)
    jmp exit_label              # 预期: 只触发 TAKEN_BRANCH

    # 这条指令永远不会被执行
    movq $888, %rdx

exit_label:
    # 6. 准备退出 (64位系统调用)
    movq $60, %rax              # sys_exit 系统调用号
    movq $0, %rdi               # exit code 0
    syscall                     # 调用内核
