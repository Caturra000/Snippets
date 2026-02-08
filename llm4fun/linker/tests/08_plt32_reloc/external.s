# 08_plt32_reloc/external.s

    .global external_func
    .text

external_func:
    mov     $44, %rax
    ret
