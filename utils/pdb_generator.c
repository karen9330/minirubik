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

static uint8_t *build_permutation_pdb(uint16_t permutation[3][PERMUTATIONS])
{
    uint16_t queue[PERMUTATIONS];
    uint16_t head = 0, tail = 1;
    uint8_t *permutation_pdb = malloc(sizeof(uint8_t) * PERMUTATIONS);

    memset(permutation_pdb, UINT8_MAX, PERMUTATIONS);

    permutation_pdb[0] = 0;
    queue[0] = 0;

    while (head < tail) {
        uint16_t p = queue[head++];
        uint8_t depth = permutation_pdb[p];

        for (uint8_t face = 0; face < 3; face++) {
            uint16_t next = p;
            for (uint8_t turn = 0; turn < 3; turn++) {
                next = permutation[face][next];
                if (permutation_pdb[next] == UINT8_MAX) {
                    permutation_pdb[next] = (uint8_t) (depth + 1);
                    queue[tail++] = next;
                }
            }
        }
    }

    return permutation_pdb;
}

static uint8_t *build_orientation_pdb(uint16_t orientation[3][ORIENTATIONS])
{
    uint16_t queue[ORIENTATIONS];
    uint16_t head = 0, tail = 1;
    uint8_t *orientation_pdb = malloc(sizeof(uint8_t) * ORIENTATIONS);

    memset(orientation_pdb, UINT8_MAX, ORIENTATIONS);

    orientation_pdb[0] = 0;
    queue[0] = 0;

    while (head < tail) {
        uint16_t o = queue[head++];
        uint8_t depth = orientation_pdb[o];

        for (uint8_t face = 0; face < 3; face++) {
            uint16_t next = o;
            for (uint8_t turn = 0; turn < 3; turn++) {
                next = orientation[face][next];
                if (orientation_pdb[next] == UINT8_MAX) {
                    orientation_pdb[next] = (uint8_t) (depth + 1);
                    queue[tail++] = next;
                }
            }
        }
    }

    return orientation_pdb;
}

static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}

int main(int argc, char **argv)
{
    uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    state_t state;

    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] = (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] = (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }

    uint8_t *permutation_pdb =  build_permutation_pdb(permutation);
    if(!permutation_pdb) {
        fprintf(stderr, "could not build complete permutation PDB\n");
        return 1;
    }

    uint8_t *orientation_pdb =  build_orientation_pdb(orientation);
    if(!orientation_pdb) {
        free(permutation_pdb);
        fprintf(stderr, "could not build complete orientation PDB\n");
        return 1;
    }

    FILE *fptr = fopen("pdb_tables.h", "w");
    if(fptr == NULL) {
        fprintf(stderr, "could not open file\n");
        free(permutation_pdb);
        free(orientation_pdb);
        return 1;
    }

    fprintf(fptr, "#include <stdint.h>\n");
    fprintf(fptr, "static const uint8_t permutation_pdb[%d] = {\n", PERMUTATIONS);
    for (int p = 0; p < PERMUTATIONS; ++p) {
        fprintf(fptr, "%u%s ", permutation_pdb[p], p == PERMUTATIONS - 1 ? "" : ",");
        if ((p + 1) % 50 == 0) fprintf(fptr, "\n");
    }
    fprintf(fptr, "\n};\n");

    fprintf(fptr, "static const uint8_t orientation_pdb[%d] = {\n", ORIENTATIONS);
    for(int o = 0; o < ORIENTATIONS; o++) {
        fprintf(fptr, "%u%s ", orientation_pdb[o], o == ORIENTATIONS - 1 ? "" : ",");
        if((o + 1) % 50 == 0) fprintf(fptr, "\n");
    }
    fprintf(fptr, "\n};\n");

    fprintf(fptr, "static uint16_t permutation_transition[%d][%d] = {\n", 3, PERMUTATIONS);
    for (uint8_t face = 0; face < 3; ++face) {
        fprintf(fptr, "    {");
        for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
            fprintf(fptr, "%u%s ", permutation[face][rank], rank == PERMUTATIONS - 1 ? "" : ",");
            if ((rank + 1) % 50 == 0) fprintf(fptr, "\n");
        }
        fprintf(fptr, "\n    }%s\n", face == 2 ? "" : ",");
    }
    fprintf(fptr, "};\n");

    fprintf(fptr, "static const uint16_t orientation_transition[3][%d] = {\n", ORIENTATIONS);
    for (uint8_t face = 0; face < 3; ++face) {
        fprintf(fptr, "    {\n");
        for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
            fprintf(fptr, "%u%s ", orientation[face][rank], rank == ORIENTATIONS - 1 ? "" : ",");
            if ((rank + 1) % 50 == 0) fprintf(fptr, "\n");
        }
        fprintf(fptr, "\n    }%s\n", face == 2 ? "" : ",");
    }
    fprintf(fptr, "};\n");

    fclose(fptr);

    free(permutation_pdb);
    free(orientation_pdb);
    
    return output_failed();
}
