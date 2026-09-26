.text
main:
    li t0, 500000

loop:
    nop
    nop
    addi t0, t0, -1
    bnez t0, loop

done:
    li a7, 10
    ecall
