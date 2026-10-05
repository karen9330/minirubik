#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];   // record the cube state.
} state_t;

/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},  // R
    {0, 1, 2, 4, 5, 6, 3},  // B
    {0, 2, 5, 3, 1, 4, 6},  // D
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static const uint8_t move_faces[9] = {
    0, 0, 0, 1, 1, 1, 2, 2, 2
};

static const uint8_t move_turns[9] = {
    1, 2, 3, 1, 2, 3, 1, 2, 3
};

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = move_turns[move];
    uint8_t face = move_faces[move];
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, face);

    return state;
}

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller; // Horner's rule
    }

    for (uint8_t i = 0; i < 6; ++i) // Change base-3 to decimal
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

static uint16_t **allocate_transition_table(size_t rows, size_t columns)
{
    uint16_t **table = malloc(sizeof(*table) * rows);

    if (table == NULL) return NULL;

    for (size_t row = 0; row < rows; ++row) {
        table[row] = malloc(sizeof(**table) * columns);

        if (table[row] == NULL) {
            for (size_t i = 0; i < row; ++i)
                free(table[i]);

            free(table);
            return NULL;
        }
    }

    return table;
}

static void free_transition_table(uint16_t **table, size_t rows)
{
    if (table == NULL) return;

    for (size_t row = 0; row < rows; ++row)
        free(table[row]);

    free(table);
}

static uint8_t *build_pdb(uint16_t num_states, uint16_t **transition)
{
    uint16_t *queue = malloc(sizeof(*queue) * num_states);
    uint8_t *pdb = malloc(sizeof(*pdb) * num_states);

    if (queue == NULL || pdb == NULL) {
        free(queue);
        free(pdb);
        return NULL;
    }

    memset(pdb, UINT8_MAX, num_states);

    size_t head = 0;
    size_t tail = 1;

    pdb[0] = 0;
    queue[0] = 0;

    while (head < tail) {
        uint16_t rank = queue[head++];
        uint8_t depth = pdb[rank];

        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next = rank;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next = transition[face][next];

                if (pdb[next] == UINT8_MAX) {
                    pdb[next] = (uint8_t)(depth + 1);
                    queue[tail++] = next;
                }
            }
        }
    }

    free(queue);
    return pdb;
}

static uint16_t **build_transition(uint16_t num_states)
{
    uint16_t **transition = allocate_transition_table(3, num_states);
    if (transition == NULL) return NULL;

    state_t state;

    for (uint16_t rank = 0; rank < num_states; ++rank) {
        if (num_states == PERMUTATIONS)
            unrank_state((uint32_t)rank * ORIENTATIONS, &state);
        else
            unrank_state(rank, &state);

        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            uint32_t next_rank = rank_state(&next);

            if (num_states == PERMUTATIONS)
                transition[face][rank] = (uint16_t)(next_rank / ORIENTATIONS);
            else
                transition[face][rank] = (uint16_t)(next_rank % ORIENTATIONS);
        }
    }

    return transition;
}

static uint16_t **build_direct_move_transition(uint16_t num_states)
{
    uint16_t **transition = allocate_transition_table(MOVES, num_states);
    if (transition == NULL) return NULL;

    state_t state;

    for (uint16_t rank = 0; rank < num_states; ++rank) {
        if (num_states == PERMUTATIONS)
            unrank_state((uint32_t)rank * ORIENTATIONS, &state);
        else
            unrank_state(rank, &state);

        for (uint8_t move = 0; move < MOVES; ++move) {
            state_t next = apply_move(state, move);
            uint32_t next_rank = rank_state(&next);

            if (num_states == PERMUTATIONS)
                transition[move][rank] = (uint16_t)(next_rank / ORIENTATIONS);
            else
                transition[move][rank] = (uint16_t)(next_rank % ORIENTATIONS);
        }
    }

    return transition;
}

static int verify_direct_move_transitions(uint16_t **permutation_transition, uint16_t **orientation_transition,
                                          uint16_t **permutation_direct, uint16_t **orientation_direct)
{
    for (uint16_t p = 0; p < PERMUTATIONS; p++) {
        for (uint8_t move = 0; move < MOVES; move++) {

            uint16_t expected = p;
            uint8_t face = move_faces[move];
            uint8_t turns = move_turns[move];

            for (uint8_t turn = 0; turn < turns; turn++)
                expected = permutation_transition[face][expected];
            
            if (expected != permutation_direct[move][p]) {

                fprintf(stderr, "permutation mismatch: " "move=%u rank=%u " "expected=%u actual=%u\n",
                        move, p, expected, permutation_direct[move][p]);
                return 0;
            }
        }
    }

    for (uint16_t o = 0; o < ORIENTATIONS; o++) {
        for (uint8_t move = 0; move < MOVES; move++) {

            uint16_t expected = o;
            uint8_t face = move_faces[move];
            uint8_t turns = move_turns[move];

            for (uint8_t turn = 0; turn < turns; turn++)
                expected = orientation_transition[face][expected];
            

            if (expected != orientation_direct[move][o]) {

                fprintf(stderr, "orientation mismatch: " "move=%u rank=%u " "expected=%u actual=%u\n",
                        move, o, expected, orientation_direct[move][o]);
                return 0;
            }
        }
    }

    return 1;
}

static void output_uint8_array(FILE *fptr, const char *name, const uint8_t *array, size_t size)
{
    fprintf(fptr, "static const uint8_t %s[%zu] = {\n", name, size);

    for (size_t i = 0; i < size; i++) {
        fprintf(fptr,"    %u%s", array[i], i + 1 == size ? "" : ",");
        if ((i + 1) % 16 == 0) fprintf(fptr, "\n");
    }

    fprintf(fptr, "\n};\n\n");
}

static void output_uint16_table(FILE *fptr, const char *name, uint16_t **table, size_t rows, size_t columns)
{
    fprintf(fptr, "static const uint16_t " "%s[%zu][%zu] = {\n", name, rows, columns);

    for (size_t row = 0; row < rows; row++) {
        fprintf(fptr, "    {\n");

        for (size_t column = 0; column < columns; column++) {
            fprintf(fptr, "        %u%s", table[row][column], column + 1 == columns ? "" : ",");
            if ((column + 1) % 12 == 0) fprintf(fptr, "\n");
        }

        fprintf(fptr, "\n    }%s\n", row + 1 == rows ? "" : ",");
    }

    fprintf(fptr, "};\n\n");
}

static int output_header_file(const char *filename, uint8_t *permutation_pdb, uint8_t *orientation_pdb,
                              uint16_t **permutation_transition, uint16_t **orientation_transition,
                              uint16_t **permutation_direct, uint16_t **orientation_direct)
{
    FILE *fptr = fopen(filename, "w");

    if (fptr == NULL) {
        fprintf(stderr, "could not open %s\n", filename);
        return 0;
    }

    output_uint8_array(fptr, "permutation_pdb", permutation_pdb, PERMUTATIONS);
    output_uint8_array(fptr, "orientation_pdb", orientation_pdb, ORIENTATIONS);
    
    output_uint16_table(fptr, "permutation_transition", permutation_transition, 3, PERMUTATIONS);
    output_uint16_table(fptr, "orientation_transition", orientation_transition, 3, ORIENTATIONS);

    output_uint16_table(fptr, "permutation_direct_move_transition", permutation_direct, MOVES, PERMUTATIONS);
    output_uint16_table(fptr, "orientation_direct_move_transition", orientation_direct, MOVES, ORIENTATIONS);


    if (fclose(fptr) != 0) {
        fprintf(stderr, "could not close %s\n", filename);
        return 0;
    }

    return 1;
}

static void output_asm_uint8_array(FILE *fptr, const char *name, const uint8_t *array, size_t size)
{
    fprintf(fptr, ".globl %s\n", name);
    fprintf(fptr, "%s:\n", name);

    for (size_t i = 0; i < size; i += 16) {
        fprintf(fptr, "    .byte ");
        size_t end = i + 16;
        if (end > size) end = size;
        for (size_t j = i; j < end; j++) {
            fprintf(fptr, "%u%s", array[j], j + 1 == end ? "" : ", ");
        }
        fprintf(fptr, "\n");
    }

    fprintf(fptr, "\n");
}

static void output_asm_uint16_table(FILE *fptr, const char *name, uint16_t **table, size_t rows, size_t columns)
{
    fprintf(fptr, ".globl %s\n", name);
    fprintf(fptr, "%s:\n", name);

    for (size_t row = 0; row < rows; row++) {
        for (size_t column = 0; column < columns; column += 12) {
            fprintf(fptr, "    .half ");
            size_t end = column + 12;
            if (end > columns) end = columns;
            for (size_t j = column; j < end; j++) {
                fprintf(fptr, "%u%s", table[row][j], j + 1 == end ? "" : ", ");
            }

            fprintf(fptr, "\n");
        }
    }

    fprintf(fptr, "\n");
}

static void output_asm_row_pointers(FILE *fptr, const char *pointer_name, const char *table_name, size_t rows, size_t columns)
{
    size_t row_bytes = columns * sizeof(uint16_t);

    fprintf(fptr, ".align 2\n");
    fprintf(fptr, ".globl %s\n", pointer_name);
    fprintf(fptr, "%s:\n", pointer_name);

    for (size_t row = 0; row < rows; ++row) {
        fprintf(fptr, "    .word %s + %zu\n", table_name, row * row_bytes);
    }

    fprintf(fptr, "\n");
}

static int output_asm_header(const char *filename, uint8_t *permutation_pdb, uint8_t *orientation_pdb,
                              uint16_t **permutation_direct, uint16_t **orientation_direct) {
    FILE *fptr = fopen(filename, "w");

    if (fptr == NULL) {
        fprintf(stderr, "could not open %s\n", filename);
        return 0;
    }
    
    fprintf(fptr, ".section .rodata\n\n");
    output_asm_uint8_array(fptr, "per_pdb", permutation_pdb, PERMUTATIONS);
    output_asm_uint8_array(fptr, "ori_pdb", orientation_pdb, ORIENTATIONS);

    output_asm_row_pointers(fptr, "per_trans_rows", "per_trans", 9, 5040);
    output_asm_row_pointers(fptr, "ori_trans_rows", "ori_trans", 9, 729);
    
    output_asm_uint16_table(fptr, "per_trans", permutation_direct, MOVES, PERMUTATIONS);
    output_asm_uint16_table(fptr, "ori_trans", orientation_direct, MOVES, ORIENTATIONS);

    if (fclose(fptr) != 0) {
        fprintf(stderr, "could not close %s\n", filename);
        return 0;
    }

    return 1;
}

int main(void)
{
    uint16_t **permutation_transition = build_transition(PERMUTATIONS);
    if (permutation_transition == NULL) {
        fprintf(stderr, "could not build permutation transition\n");
        return 1;
    }

    uint16_t **orientation_transition = build_transition(ORIENTATIONS);
    if (orientation_transition == NULL) {
        fprintf(stderr, "could not build orientation transition\n");
        free_transition_table(permutation_transition, 3);
        return 1;
    }

    uint8_t *permutation_pdb = build_pdb(PERMUTATIONS, permutation_transition);
    if (permutation_pdb == NULL) {
        fprintf(stderr, "could not build permutation PDB\n");
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        return 1;
    }

    uint8_t *orientation_pdb = build_pdb(ORIENTATIONS, orientation_transition);
    if (orientation_pdb == NULL) {
        fprintf(stderr, "could not build orientation PDB\n");
        free(permutation_pdb);
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        return 1;
    }

    uint16_t **permutation_direct = build_direct_move_transition(PERMUTATIONS);
    if (permutation_direct == NULL) {
        fprintf(stderr, "could not build permutation direct transition\n");
        free(permutation_pdb);
        free(orientation_pdb);
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        return 1;
    }

    uint16_t **orientation_direct = build_direct_move_transition(ORIENTATIONS);
    if (orientation_direct == NULL) {
        fprintf(stderr, "could not build orientation direct transition\n");
        free(permutation_pdb);
        free(orientation_pdb);
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        free_transition_table(permutation_direct, MOVES);
        return 1;
    }

    if (!verify_direct_move_transitions(permutation_transition, orientation_transition, permutation_direct, orientation_direct)) {
        fprintf(stderr, "direct transition verification failed\n");
        free(permutation_pdb);
        free(orientation_pdb);
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        free_transition_table(permutation_direct, MOVES);
        free_transition_table(orientation_direct, MOVES);
        return 1;
    }

    printf("verified all direct move transitions\n");

    if (!output_header_file("pdb_tables.h", permutation_pdb, orientation_pdb, permutation_transition, orientation_transition, permutation_direct, orientation_direct)) {
        free(permutation_pdb);
        free(orientation_pdb);
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        free_transition_table(permutation_direct, MOVES);
        free_transition_table(orientation_direct, MOVES);
        return 1;
    }

    if (!output_asm_header("pdb_tables.s", permutation_pdb, orientation_pdb, permutation_direct, orientation_direct)) {
        free(permutation_pdb);
        free(orientation_pdb);
        free_transition_table(permutation_transition, 3);
        free_transition_table(orientation_transition, 3);
        free_transition_table(permutation_direct, MOVES);
        free_transition_table(orientation_direct, MOVES);
        return 1;
    }

    free(permutation_pdb);
    free(orientation_pdb);
    free_transition_table(permutation_transition, 3);
    free_transition_table(orientation_transition, 3);
    free_transition_table(permutation_direct, MOVES);
    free_transition_table(orientation_direct, MOVES);

    printf("generated pdb_tables.h\n");
    return 0;
}
