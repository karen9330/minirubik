#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    MAX_DEPTH = 11,
    NO_FACE = 3
};

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

typedef struct {
    uint8_t p[CUBIES];
    uint8_t o[CUBIES];
} state_t;

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
    uint8_t *exact_dist = build_exact_distances();

    if (!exact_dist) {
        fprintf(stderr, "could not build exact distance table\n");
        return 1;
    }

    uint32_t count = 0;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        if (exact_dist[rank] != 11)
            continue;

        state_t state;
        unrank_state(rank, &state);

        for (int i = 0; i < 14; i++) {
            if (i < 7) printf("%c", state.p[i] + '1');
            else printf("%c", state.o[i - 7] + '1');
        }

        printf("\n");
        ++count;
    }

    fprintf(stderr, "exported %u distance-11 states\n", count);

    free(exact_dist);

    return count == 2644 ? 0 : 1;
}