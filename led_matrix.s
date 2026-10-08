.section .rodata
.align 2

.global cubie_home_colors
cubie_home_colors:
    .byte 0, 2, 1
    .byte 0, 3, 2
    .byte 5, 2, 3
    .byte 5, 1, 2
    .byte 0, 4, 3
    .byte 5, 3, 4
    .byte 5, 4, 1
    .byte 0, 1, 4

.global position_to_facelet_indices
position_to_facelet_indices:
    .byte 2, 8, 5
    .byte 3, 12, 9
    .byte 21, 11, 14
    .byte 20, 7, 10
    .byte 1, 16, 13
    .byte 23, 15, 18
    .byte 22, 19, 6
    .byte 0, 4, 17

.globl color_lookup
color_lookup:
    .word 0x00FFFFFF, 0x00FFA500, 0x0000FF00
    .word 0x00FF0000, 0x000000FF, 0x00FFFF00

.global face_x
face_x:
    .byte 9, 0, 9, 18, 27, 9

.global face_y
face_y:
    .byte 2, 9, 9, 9, 9, 16

.global slot_dx
slot_dx:
    .byte 0, 4, 0, 4

.global slot_dy
slot_dy:
    .byte 0, 0, 3, 3

.data
.globl buffer
buffer:
    .zero 24

.text
.globl render_state
render_state:
    # input
    # a0 = full state base
    # a0 + 0..6  = p
    # a0 + 7..13 = o
    addi sp, sp, -64
    sw ra, 0(sp)
    sw s1, 4(sp)
    sw s2, 8(sp)
    sw s3, 12(sp)
    sw s4, 16(sp)
    sw s5, 20(sp)
    sw s6, 24(sp)
    sw s7, 32(sp)
    sw s8, 36(sp)
    sw s9, 40(sp)
    sw s10, 44(sp)
    sw s11, 48(sp)

    li s1, LED_MATRIX_0_BASE
    la s2, buffer
    la s3, color_lookup
    la s4, face_x
    la s5, face_y
    la s6, slot_dx
    la s7, slot_dy
    la s10, cubie_home_colors
    la s11, position_to_facelet_indices
    li s8, 8        # s8 = outer loop end = 8
    li s9, 3        # s9 = inner loop end = 3
    li t0, 0        # t0 = position = 0

build_facelet_buffer:
    beq t0, s8, draw_buffer
    li t1, 0        # t1 = k = 0

    bnez t0, not_position_zero  # if position == 0
    li t2, 0    # t2 = cubie = 0
    li t3, 0    # t3 = ori = 0
    j store_loop

not_position_zero:
    addi t5, t0, -1     # t5 = position - 1
    add t6, t5, a0      # t6 = &p[position - 1]
    lbu t2, 0(t6)       # t2 = cubie = p[position - 1]
    addi t2, t2, 1      # t2++ change into physical cubie

    addi t4, a0, 7      # t4 = &o[0]
    add t4, t5, t4      # t4 = &o[position - 1]
    lbu t3, 0(t4)       # t3 = ori = o[position - 1]

store_loop:
    beq t1, s9, next_position
    add t4, t3, t1      # t4 = ori + k = color_idx
    li t5, 3
    bltu t4, t5, no_adjust
    addi t4, t4, -3     # t4 = t4 - 3
no_adjust:
    slli t5, t2, 1      # t5 = cubie * 2
    add t5, t5, t2      # t5 = cubie * 3
    add t5, t5, t4      # t5 = cubie * 3 + color_index
    
    add t5, t5, s10     # t5 = &cubie_home_colors[cubie * 3 + color_index]
    lbu t6, 0(t5)       # t6 = color
    
    slli t5, t0, 1
    add t5, t5, t0      # t5 = position * 3
    add t5, t5, t1      # t5 = position * 3 + k
    add t5, t5, s11     # t5 = &position_to_facelet_indices[position * 3 + k]
    lbu t4, 0(t5)       # t4 = dst

    add t5, s2, t4      # t5 = &buffer[dst]
    sb t6, 0(t5)        # buffer[dst] = color
    addi t1, t1, 1      # k++
    j store_loop
next_position:
    addi t0, t0, 1      # position++
    j build_facelet_buffer

draw_buffer:
    li s8, 0        # s8 = face = 0

face_loop:
    li t0, 6        # t0 = end = 6
    beq t0, s8, render_state_done
    li s9, 0        # slot = 0
slot_loop:
    li t0, 4        # t0 = end = 4
    beq t0, s9, next_face
    slli t0, s8, 2  # t0 = face * 4
    add t0, t0, s9  # t0 = t0 + slot = index

    add t0, t0, s2  # t0 = &buffer[index]
    lbu t0, 0(t0)   # t0 = buffer[index] = color_id

    slli t0, t0, 2  # t0 = color_id * 4
    add t0, t0, s3  # t0 = &color_lookup[color_id]
    lw a2, 0(t0)    # a2 = color_lookup[color_id] = rgb

    add t0, s8, s4  # t0 = &face_x[face]
    lbu t1, 0(t0)   # t1 = face_x[face]

    add t0, s9, s6  # t0 = &slot_dx[slot]
    lbu t2, 0(t0)   # t2 = slot_dx[slot]

    add a0, t1, t2  # a0 = face_x[face] + slot_dx[slot] = x

    add t0, s8, s5  # t0 = &face_y[face]
    lbu t1, 0(t0)   # t1 = face_y[face]

    add t0, s9, s7  # t0 = &slot_dy[slot]
    lbu t2, 0(t0)   # t2 = slot_dy[slot]

    add a1, t1, t2  # a1 = face_y[face] + slot_dy[slot] = y
    jal ra, draw_facelet
    addi s9, s9, 1  # slot++
    j slot_loop
next_face:
    addi s8, s8, 1  # face++
    j face_loop

# draw_facelet(x0, y0, color)
draw_facelet:
    li a3, 3    # row = 3
    li a4, 4    # col = 4
    li t3, 0    # t3 = i = 0
    
outer_loop:
    beq t3, a3, outer_loop_done
    add t6, t3, a1    # t6 = y0 + i
    li t4, 0          # t4 = j = 0
    
inner_loop:
    beq t4, a4, inner_loop_done
    add t5, t4, a0    # t5 = x0 + j
    slli t1, t6, 5    # t1 = y * 32
    add t1, t6, t1    # t1 = y * 33
    add t1, t6, t1    # t1 = y * 34
    add t1, t6, t1    # t1 = y * 35
    
    add t1, t1, t5   # t1 = y * 35 + x
    slli t1, t1, 2    # t1 *= 4
    
    add t1, t1, s1    # address of LED
    sw a2, 0(t1)      # light up LED
    addi t4, t4, 1    # j++
    j inner_loop
    
inner_loop_done:
    addi t3, t3, 1    # i++
    j outer_loop
outer_loop_done:
    ret

render_state_done:
    lw ra, 0(sp)
    lw s1, 4(sp)
    lw s2, 8(sp)
    lw s3, 12(sp)
    lw s4, 16(sp)
    lw s5, 20(sp)
    lw s6, 24(sp)
    lw s7, 32(sp)
    lw s8, 36(sp)
    lw s9, 40(sp)
    lw s10, 44(sp)
    lw s11, 48(sp)
    addi sp, sp, 64
    ret