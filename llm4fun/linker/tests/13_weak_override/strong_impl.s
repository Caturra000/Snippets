# 13_weak_override/strong_impl.s

    .global get_number
    .text

get_number:
    mov     $222, %rax
    ret
