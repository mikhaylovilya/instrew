    .text
    .global _start
_start:
    mv a0, sp
1:  ld a5, (a0)
2:  lr.d.aqrl a4, (a0)
    bne a4, a5, 1b
    sc.d.rl a2,a3,(a0)
    bnez a2, 2b
    bne a4, a5, 1b

    li a0, 0
    li a7, 94
    ecall
    j .
