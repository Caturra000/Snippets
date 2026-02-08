# 07_pc32_reloc/target.s

    .global target
    .text

target:
    mov     $33, %rax
    ret
