# 12_dup_error/file2.s

    .global duplicate_sym
    .text

duplicate_sym:
    mov     $2, %rax
    ret
