.globl _start
_start:
    jal ra, main

    # main returns a0:
    # 0 = success
    # 1 = search/self-test failure
    # 2 = parse failure

    li a7, 93
    ecall
    