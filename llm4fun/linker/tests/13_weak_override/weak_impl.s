# 13_weak_override/weak_impl.s

    .weak   get_number
    .text

get_number:
    mov     $111, %rax
    ret
