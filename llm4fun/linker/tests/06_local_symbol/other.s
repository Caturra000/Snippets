# 06_local_symbol/other.s

    .global dummy
    .text

helper:
    mov     $99, %rax
    ret

dummy:
    ret
