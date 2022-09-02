There are 13 section headers, starting at offset 0x350:

Section Headers:
  [Nr] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  [ 0]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  [ 1] .text             PROGBITS         0000000000000000  00000040
       000000000000003e  0000000000000000  AX       0     0     1
  [ 2] .rela.text        RELA             0000000000000000  00000268
       0000000000000048  0000000000000018   I      10     1     8
  [ 3] .data             PROGBITS         0000000000000000  00000080
       0000000000000004  0000000000000000  WA       0     0     4
  [ 4] .bss              NOBITS           0000000000000000  00000084
       0000000000000000  0000000000000000  WA       0     0     1
  [ 5] .comment          PROGBITS         0000000000000000  00000084
       000000000000002b  0000000000000001  MS       0     0     1
  [ 6] .note.GNU-stack   PROGBITS         0000000000000000  000000af
       0000000000000000  0000000000000000           0     0     1
  [ 7] .note.gnu.propert NOTE             0000000000000000  000000b0
       0000000000000020  0000000000000000   A       0     0     8
  [ 8] .eh_frame         PROGBITS         0000000000000000  000000d0
       0000000000000058  0000000000000000   A       0     0     8
  [ 9] .rela.eh_frame    RELA             0000000000000000  000002b0
       0000000000000030  0000000000000018   I      10     8     8
  [10] .symtab           SYMTAB           0000000000000000  00000128
       0000000000000120  0000000000000018          11    11     8
  [11] .strtab           STRTAB           0000000000000000  00000248
       000000000000001e  0000000000000000           0     0     1
  [12] .shstrtab         STRTAB           0000000000000000  000002e0
       000000000000006c  0000000000000000           0     0     1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
  L (link order), O (extra OS processing required), G (group), T (TLS),
  C (compressed), x (unknown), o (OS specific), E (exclude),
  l (large), p (processor specific)
