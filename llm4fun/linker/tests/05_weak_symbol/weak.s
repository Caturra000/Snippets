# 05_weak_symbol/weak.s

    .weak   get_value
    .text

get_value:
    mov     $77, %rax
    ret
