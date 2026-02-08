# 02_multi_obj/func.s

    .global get_exit_code
    .text

get_exit_code:
    mov     $42, %rax
    ret
