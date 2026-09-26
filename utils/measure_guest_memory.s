.text
main:
    li t0, 0x20000000   # unused memory region
    li t1, 2097152     # iteration times (0, 1MiB: 262144,  4MiB: 1048576, 8MiB: 2097152)

write_loop:
    beqz t1, done   # check when to finish

    sw zero, 0(t0)
    addi t0, t0, 4
    addi t1, t1, -1     # iteration times--
    j write_loop

done:
    li a7, 1
    ecall               # print 1 when finish