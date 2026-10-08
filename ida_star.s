.equ RENDER, 1
.text
.globl main

main:
    addi sp, sp, -176

    sw s0, 128(sp)
    sw s1, 132(sp)
    sw s2, 136(sp)
    sw s3, 140(sp)
    sw s4, 144(sp)
    sw s5, 148(sp)
    sw s6, 152(sp)
    sw s7, 156(sp)
    sw s8, 160(sp)
    sw s9, 164(sp)
    sw s10, 168(sp)

.if RENDER
    sw ra, 172(sp)         # need to call function render_state
.endif

    addi s0, sp, 20        # s0 = path base
    la s2, per_pdb         # s2 = &per_pdb
    la s3, ori_pdb         # s3 = &ori_pdb
    la s4, per_trans_rows  # s4 = &per_trans_rows
    la s5, ori_trans_rows  # s5 = &ori_trans_rows
    la s6, move_faces      # s6 = base address of move_faces
    la t0, input_state     # t0 = &input_state

parse_perm:
    li t1, 0             # i = 0
    li a0, 7             # a0 = end = 7

parse_perm_loop:
    beq t1, a0, parse_ori

    add t3, t0, t1       # t3 = &input[i]
    lbu t2, 0(t3)        # t2 = input[i]

    li t4, 49            # check input[i] >= '1'
    bltu t2, t4, parse_fail

    li t4, 55             # check input[i] <= '7'
    bltu t4, t2, parse_fail

    addi t2, t2, -49      # convert ASCII '1'..'7' → 0..6

    add t3, sp, t1        # store p[i] at sp+i
    sb t2, 0(t3)

    addi t1, t1, 1
    j parse_perm_loop

parse_ori:
    li t1, 0             # i = 0

parse_ori_loop:
    beq t1, a0, parse_done

    addi t3, t0, 7       # load input[7 + i]
    add t3, t3, t1
    lbu t2, 0(t3)

    li t4, 49            # check >= '1'
    bltu t2, t4, parse_fail

    li t4, 51            # check <= '3'
    bltu t4, t2, parse_fail

    addi t2, t2, -49    # convert '1'..'3' -> 0..2

    addi t3, sp, 7      # store o[i] at sp + 7 + i
    add t3, t3, t1
    sb t2, 0(t3)

    addi t1, t1, 1      # i++
    j parse_ori_loop

parse_done:
    lbu t1, 14(t0)      # t1 = input[2 * CUBIES]
    bnez t1, parse_fail # if t1 != '\0'; goto parse_fail

validate_perm:
    li t1, 1             # i = 1

valid_outer:
    beq t1,a0, validate_ori_sum

    add t6, sp, t1       # t6 = &p[i]
    lbu t3, 0(t6)        # t3 = p[i]

    li t2, 0             # j = 0

valid_inner:
    beq t2, t1, valid_next_i

    add t6, sp, t2       # t6 = &p[j]
    lbu t4, 0(t6)        # t4 = p[j]

    beq t4, t3, parse_fail  # if p[j] == p[i]; goto parse_fail

    addi t2, t2, 1      # j++
    j valid_inner

valid_next_i:
    addi t1, t1, 1      # i++
    j valid_outer

validate_ori_sum:
    li t1, 0            # i = 0
    li t2, 0            # sum = 0

ori_sum_loop:
    beq t1, a0, ori_mod3

    addi t3, sp, 7      # t3 = &o[0]
    add t3, t3, t1      # t3 = &o[i]
    lbu t3, 0(t3)       # t3 = o[i]

    add t2, t2, t3      # sum += o[i]

    addi t1, t1, 1      # i++
    j ori_sum_loop

ori_mod3:
    li t3, 3            # t3 = 3
    bltu t2, t3, ori_valid_check

    addi t2, t2, -3
    j    ori_mod3

ori_valid_check:
    bnez t2, parse_fail
    addi t0, sp, 0      # p base
    addi t6, sp, 14     # digits base

rank_p_digits:
    li t1, 0            # i = 0

digit_outer:
    li t5, 6            # compare i with 6
    beq t1, t5, digits_done

    # load p[i]
    add t5, t0, t1      # t5 = &p[i]
    lbu t3, 0(t5)       # t3 = p[i]

    li t4, 0            # smaller = 0

    addi t2, t1, 1      # j = i + 1

digit_inner:
    beq t2, a0, store_digit

    add t5, t0, t2      # t5 = &p[j]
    lbu t5, 0(t5)       # t5 = p[j]

    bltu t5, t3, digit_smaller  # if p[j] < p[i], smaller++

digit_next:
    addi t2, t2, 1      # j++
    j digit_inner

digit_smaller:
    addi t4, t4, 1      # smaller++
    j digit_next

store_digit:
    # digits[i] = smaller
    add t5, t6, t1      # t5 = &digits[i]
    sb t4, 0(t5)

    addi t1, t1, 1      # i++
    j digit_outer

digits_done:
    lbu t1, 0(t6)        # t1 = d0

    # rank = rank * 6 + d1
    slli t3, t1, 1
    slli t1, t1, 2
    add t1, t1, t3
    lbu t2, 1(t6)         # t2 = d1
    add t1, t1, t2

    # rank = rank * 5 + d2
    slli t3, t1, 2
    add t1, t1, t3
    lbu t2, 2(t6)
    add t1, t1, t2

    # rank = rank * 4 + d3
    slli t1, t1, 2
    lbu t2, 3(t6)
    add t1, t1, t2

    # rank = rank * 3 + d4
    slli t3, t1, 1
    add t1, t1, t3
    lbu t2, 4(t6)
    add t1, t1, t2

    # rank = rank * 2 + d5
    slli t1, t1, 1
    lbu t2, 5(t6)
    add s10, t1, t2  # s10 = t1 = permutation rank
    addi t0, sp, 7   # t0 = &o[0]

rank_o_digits:
    li t3, 0        # t3 = i = 0
    li s1, 0        # s1 = o = 0
    li t6, 6        # t6 = 6 = end

rank_o_loop:
    beq t6, t3, search_setup
    add t4, t0, t3  # t4 = &o[i]
    lbu t4, 0(t4)   # t4 = o[i]
    slli t5, s1, 1  # t5 = o * 2
    add t5, s1, t5  # t5 = o * 3
    add s1, t5, t4  # s1 = o' = o * 3 + o[i] = orientation rank
    addi t3, t3, 1  # i++
    j rank_o_loop

search_setup:
    add t0, s10, s2     # t0 = &per_pdb[start_p]
    lbu s8, 0(t0)       # s8 = hp = per_pdb[start_p]
    add t0, s1, s3     # t0 = &ori_pdb[start_o]
    lbu t0, 0(t0)       # t0 = ho = ori_pdb[start_o]
    bgeu s8, t0, outer_loop     # if hp >= ho; s8 = hp = bound
    mv s8, t0           # otherwise s8 = ho = bound

outer_loop:
    li t0, 11           # t0 = MAX_DEPTH = 11
    bltu t0, s8, search_fail   # if bound > 11; goto search_fail
    li s7, 0            # s7 = depth = 0 (initialize)
    li s9, 255          # s9 = next_bound = 255 (initialize)

    addi t0, sp, 32      # t0 = &frame[0]
    sh s10, 0(t0)        # frame[0].p = start_p
    sh s1, 2(t0)        # frame[0].o = start_o
    sb zero, 4(t0)       # frame[0].next_move = 0
    li t1, 3
    sb t1, 5(t0)         # frame[0].last_face = NO_FACE

inner_loop:
    slli t5, s7, 3      # t5 = depth * 8
    addi t0, sp, 32     # search frame base
    add  t0, t0, t5     # t0 = &frame[depth]

    lhu  t1, 0(t0)      # t1 = current p
    lhu  t2, 2(t0)      # t2 = current o

    add  t5, s2, t1     # t5 = &per_pdb[p]
    lbu  t3, 0(t5)      # t3 = hp

    add  t5, s3, t2     # t5 = &ori_pdb[o]
    lbu  t4, 0(t5)      # t4 = ho

    bgeu t3, t4, cal_f  # if hp >= ho, keep t3
    mv  t3, t4          # otherwise t3 = ho

cal_f:
    add  t3, t3, s7     # t3 = f = h + depth

    bgeu s8, t3, keep_explore        # if bound >= f, goto keep_explore
    
    bgeu t3, s9, prune_backtrack     # if f >= next_bound, goto prune_backtrack
    mv s9, t3           # s9 = t3 update the next_bound

prune_backtrack:
    beqz s7, iteration_done  # depth == 0, stop inner_loop
    addi s7, s7, -1     # depth--
    j inner_loop        # continue

keep_explore:
    bnez t1, check_depth    # if current p != 0, not solved
    beqz t2, search_found          # p == 0; if o == 0, solved

check_depth:
    li   t4, 11         # t4 = 11 = MAX_DEPTH
    bne  s7, t4, try_moves  # if depth != MAX_DEPTH, goto try_moves
    addi s7, s7, -1     # depth--
    j inner_loop        # continue

try_moves:
    lbu t5, 4(t0)       # t5 = move = stack[depth].next_move
    li t4, 9            # t4 = 9 = MOVES
    bne t5, t4, choose_move  # if t5 != t4, some probable moves have not been tried
    beqz s7, iteration_done  # depth == 0, stop inner_loop
    addi s7, s7, -1     # depth--
    j inner_loop        # continue

iteration_done:
    li t0, 255
    beq s9, t0, search_fail    # if next_bound == 255; goto search_fail
    mv s8, s9           # bound = next_bound
    j outer_loop

choose_move:
    addi t4, t5, 1      # t4 = move + 1
    sb t4, 4(t0)        # stack[depth].next_move = move + 1
    add t4, s6, t5      # t4 = &move_faces[move]
    lbu t4, 0(t4)       # t4 = face
    lbu t3, 5(t0)       # t3 = stack[depth].last_face
    bne t4, t3, lookup_tri_table    # if t4 != t3, expand this move
    j inner_loop        # otherwise skip same-face move

lookup_tri_table:
    slli t6, t5, 2      # t6 = move * 4

    add t3, t6, s4      # t3 = &per_trans_rows[move]
    lw t3, 0(t3)        # t3 = &per_trans[move][0]
    slli t1, t1, 1      # t1 = current p * 2
    add t1, t1, t3      # t1 = &per_trans[move][current_p]
    lhu t3, 0(t1)       # t3 = child_p

    add t6, t6, s5      # t6 = &ori_trans_rows[move]
    lw   t6, 0(t6)      # t6 = &ori_trans[move][0]
    slli t2, t2, 1      # t2 = current o * 2
    add t2, t2, t6      # t2 = &ori_row_base[move][current o]
    lhu t6, 0(t2)       # t6 = child_o

    # Save move in path
    add t1, s0, s7      # t1 = &path[depth]
    sb t5, 0(t1)        # path[depth] = t5 = move

    # Push child frame
    addi s7, s7, 1      # depth++

    slli t1, s7, 3      # t1 = depth * 8
    addi t0, sp, 32     # search frame base
    add  t0, t0, t1     # t0 = &frame[depth]

    sh t3, 0(t0)        # stack[depth].p = child_p;
    sh t6, 2(t0)        # stack[depth].o = child_o;
    sb zero, 4(t0)      # stack[depth].next_move = 0;
    sb t4, 5(t0)        # stack[depth].last_face = face;
    j inner_loop

parse_fail:
    li a0, 2
    j main_epilogue

search_fail:
    li a0, 1
    j main_epilogue

search_found:
    la t0, expected_length    # t0 = &expected_length
    lbu t0, 0(t0)             # t0 = expected_length
    bne t0, s7, search_fail   # if solution_length != expected_length; goto search_fail

self_test_setup:
    addi t1, sp, 0            # t1 = check_state base
    addi t2, sp, 32           # t2 = next_state base
    
.if RENDER
    sw t1, 48(sp)             # save t1 and t2
    sw t2, 52(sp)

    mv a0, t1
    jal ra, render_state

    lw t1, 48(sp)
    lw t2, 52(sp)
.endif

    li s8, 0                  # s8 = path index = 0
replay_move_loop:
    beq s8, s7, replay_done   # if path_index == solution_length; goto replay_done
    add t3, s0, s8            # t3 = &path[s8]
    lbu t3, 0(t3)             # t3 = path[s8]
    
    add t4, s6, t3            # t4 = &move_faces[move]
    lbu s9, 0(t4)             # s9 = face
    la t5, move_turns         # t5 = &move_turns
    add t5, t5, t3            # t5 = &move_turns[move]
    lbu s10, 0(t5)            # s10 = turns

quarter_turn_loop:
    beqz s10, move_done
    li t6, 0                  # t6 = i = 0
    li s1, 7                    # s1 = CUBIES = 7

quarter_corner_loop:
    beq t6, s1, quarter_turn_done

    slli t3, s9, 3            # t3 = face * 8
    sub t3, t3, s9            # t3 = face * 7
    add t3, t3, t6            # t3 = face * 7 + i

    la t4, source             # t4 = &source
    add t4, t4, t3            # t4 = &source[face][i]
    lbu a0, 0(t4)             # a0 = src = source[face][i]

    add t5, t1, a0            # t5 = &current.p[src]
    lbu t5, 0(t5)             # t5 = current.p[src]
    add t4, t2, t6            # t4 = &next.p[i]
    sb t5, 0(t4)              # next.p[i] = current.p[src]

    addi t4, t1, 7            # t4 = &current.o[0]
    add t4, t4, a0            # t4 = &current.o[src]
    lbu t4, 0(t4)             # t4 = current.o[src]

    la t5, twist              # t5 = &twist
    add t5, t5, t3            # t5 = &twist[face][i]
    lbu t5, 0(t5)             # t5 = twist[face][i]

    add t4, t4, t5            # t4 = ori = current.o[src] + twist[face][i]
    li t5, 3                  # t5 = 3
    bltu t4, t5, ori_ready    # if ori < 3; no adjustment
    addi t4, t4, -3           # otherwise, ori -= 3

ori_ready:
    addi t5, t2, 7            # t5 = &next.o[0]
    add t5, t5, t6            # t5 = &next.o[i]
    sb t4, 0(t5)              # next.o[i] = ori
    addi t6, t6, 1            # i++
    j quarter_corner_loop

quarter_turn_done:
    mv a0, t1                 # swap current_state and next_state
    mv t1, t2
    mv t2, a0
    addi s10, s10, -1         # remaining turns--
    j quarter_turn_loop

move_done:
.if RENDER
    sw t1, 48(sp)             # save t1 and t2
    sw t2, 52(sp)

    mv a0, t1
    jal ra, render_state

    lw t1, 48(sp)
    lw t2, 52(sp)
.endif

    addi s8, s8, 1            # path_index++
    j replay_move_loop

replay_done:
    li t6, 0                  # t6 = 0 = i
    li s1, 7                  # end of permutation = 7

check_perm_loop:
    beq t6, s1, check_ori_start

    add t0, t6, t1            # t0 = &current.p[i]
    lbu t0, 0(t0)             # t0 = current.p[i]
    bne t0, t6, self_test_fail

    addi t6, t6, 1
    j check_perm_loop

check_ori_start:
    li s1, 14

check_ori_loop:
    beq t6, s1, self_test_pass

    add t0, t6, t1            # t0 = &current.o[i]
    lbu t0, 0(t0)             # t0 = current.o[i]
    bnez t0, self_test_fail

    addi t6, t6, 1
    j check_ori_loop

self_test_fail:
    li a0, 1
    j main_epilogue
    
self_test_pass:
    li a0, 0
    j main_epilogue

main_epilogue:
    lw s0, 128(sp)
    lw s1, 132(sp)
    lw s2, 136(sp)
    lw s3, 140(sp)
    lw s4, 144(sp)
    lw s5, 148(sp)
    lw s6, 152(sp)
    lw s7, 156(sp)
    lw s8, 160(sp)
    lw s9, 164(sp)
    lw s10, 168(sp)
.if RENDER
    lw ra, 172(sp)
.endif
    addi sp, sp, 176
    ret
