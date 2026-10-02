#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdb_tables.h"

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

typedef struct {
    uint8_t p[CUBIES];
    uint8_t o[CUBIES];
} state_t;

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
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

static uint8_t heuristic_rank(uint32_t rank)
{
    uint16_t p = (uint16_t)(rank / ORIENTATIONS);
    uint16_t o = (uint16_t)(rank % ORIENTATIONS);

    uint8_t hp = permutation_pdb[p];
    uint8_t ho = orientation_pdb[o];

    return hp > ho ? hp : ho;
}

static int verify_pdb_tables(void)
{
    uint32_t p_count = 0;
    uint32_t o_count = 0;
    uint8_t max_p = 0;
    uint8_t max_o = 0;

    for (uint16_t i = 0; i < PERMUTATIONS; ++i) {
        uint8_t d = permutation_pdb[i];

        if (d == UINT8_MAX) {
            fprintf(stderr, "missing permutation PDB entry %u\n", i);
            return 0;
        }

        ++p_count;
        if (d > max_p) max_p = d;
    }

    for (uint16_t i = 0; i < ORIENTATIONS; ++i) {
        uint8_t d = orientation_pdb[i];

        if (d == UINT8_MAX) {
            fprintf(stderr, "missing orientation PDB entry %u\n", i);
            return 0;
        }

        ++o_count;

        if (d > max_o) max_o = d;
    }

    printf("Permutation PDB:\n");
    printf("  populated: %u / %u\n", p_count, PERMUTATIONS);
    printf("  solved entry: %u\n", permutation_pdb[0]);
    printf("  max distance: %u\n", max_p);

    printf("Orientation PDB:\n");
    printf("  populated: %u / %u\n", o_count, ORIENTATIONS);
    printf("  solved entry: %u\n", orientation_pdb[0]);
    printf("  max distance: %u\n", max_o);

    return p_count == PERMUTATIONS &&
           o_count == ORIENTATIONS &&
           permutation_pdb[0] == 0 &&
           orientation_pdb[0] == 0 &&
           max_p == 7 &&
           max_o == 6;
}

static uint8_t *build_exact_distances(void)
{
    uint8_t *dist = malloc(STATES);
    uint32_t *queue =
        malloc((size_t)STATES * sizeof(*queue));

    if (!dist || !queue) {
        free(dist);
        free(queue);
        return NULL;
    }

    memset(dist, UINT8_MAX, STATES);

    dist[0] = 0;
    queue[0] = 0;

    uint32_t head = 0;
    uint32_t tail = 1;

    uint16_t permutation[3][PERMUTATIONS];
    uint16_t orientation[3][ORIENTATIONS];

    state_t state;
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t)rank * ORIENTATIONS, &state);

        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);

            permutation[face][rank] =
                (uint16_t)(rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);

        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);

            orientation[face][rank] =
                (uint16_t)(rank_state(&next) % ORIENTATIONS);
        }
    }

    while (head < tail) {
        uint32_t here = queue[head++];

        uint16_t p = (uint16_t)(here / ORIENTATIONS);
        uint16_t o = (uint16_t)(here % ORIENTATIONS);

        uint8_t depth = dist[here];

        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p;
            uint16_t next_o = o;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];

                uint32_t there = (uint32_t)next_p * ORIENTATIONS + next_o;

                if (dist[there] == UINT8_MAX) {
                    dist[there] = (uint8_t)(depth + 1);
                    queue[tail++] = there;
                }
            }
        }
    }

    if (tail != STATES) {
        fprintf(stderr, "BFS incomplete: %u / %u states\n", tail, STATES);

        free(dist);
        free(queue);
        return NULL;
    }

    free(queue);
    return dist;
}

int main(void)
{
    if (!verify_pdb_tables()) {
        fprintf(stderr, "PDB verification failed\n");
        return 1;
    }
    
    uint8_t *exact_dist = build_exact_distances();

    if (!exact_dist) {
        fprintf(stderr, "could not build exact distance table\n");
        return 1;
    }

    uint32_t violations = 0;
    uint32_t gap_count[12] = {0};

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint8_t h = heuristic_rank(rank);
        uint8_t d = exact_dist[rank];

        if (h > d) {
            ++violations;
            if (violations <= 10) {
                printf("violation at rank %u: h=%u d=%u\n", rank, h, d);
            }
        }
        else {
            ++gap_count[d - h];
        }
    }

    for (uint8_t gap = 0; gap <= 11; ++gap) {
        if (gap_count[gap]) printf("gap %u: %u states\n", gap, gap_count[gap]);
    }

    printf("checked %u states\n", STATES);
    printf("admissibility violations: %u\n", violations);

    free(exact_dist);

    return violations == 0 ? 0 : 1;
}