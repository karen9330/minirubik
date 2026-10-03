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
    MOVES = 9,
    MAX_DEPTH = 11,
    NO_FACE = 3
};

#define TEST_LIMIT STATES

typedef struct {
    uint8_t p[CUBIES];
    uint8_t o[CUBIES];
} state_t;

typedef struct {
    state_t state;
    uint16_t p;
    uint16_t o;
    
    uint8_t next_move;
    uint8_t last_face;
} frame_t;

static const state_t solved = {
    {0, 1, 2, 3, 4, 5, 6},
    {0, 0, 0, 0, 0, 0, 0}
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

static const uint8_t move_faces[9] = {
    0, 0, 0, 1, 1, 1, 2, 2, 2
};

static const uint8_t move_turns[9] = {
    1, 2, 3, 1, 2, 3, 1, 2, 3
};

static uint16_t rank_permutation(const state_t *state)
{
    uint16_t p = 0;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;

        for (uint8_t j = (uint8_t)(i + 1U); j < CUBIES; ++j) {
            if (state->p[j] < state->p[i])
                ++smaller;
        }

        p = p * (CUBIES - i) + smaller;
    }

    return p;
}

static uint16_t rank_orientation(const state_t *state)
{
    uint16_t o = 0;

    for (uint8_t i = 0; i < 6; ++i) 
        o = o * 3U + state->o[i];
    return o;
}

// Used in 3-quarter-turn transition table
static void apply_ranked_move(uint16_t p, uint16_t o, uint8_t move, uint16_t *next_p, uint16_t *next_o)
{
    uint8_t turns = move_turns[move];
    uint8_t face = move_faces[move];

    *next_p = p;
    *next_o = o;

    for (uint8_t i = 0; i < turns; ++i) {
        *next_p = permutation_transition[face][*next_p];
        *next_o = orientation_transition[face][*next_o];
    }
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

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, (uint8_t) (move / 3U));
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

static uint8_t heuristic(uint16_t p, uint16_t o)
{
    uint8_t hp = permutation_pdb[p];
    uint8_t ho = orientation_pdb[o];

    return hp > ho ? hp : ho;
}

static int ida_iteration(uint16_t start_p, uint16_t start_o, uint8_t bound, uint8_t *path, uint8_t *solution_length, uint8_t *next_bound, uint64_t *nodes)
{
    frame_t stack[MAX_DEPTH + 1];
    uint8_t depth = 0;

    stack[0].p = start_p;
    stack[0].o = start_o;
    stack[0].next_move = 0;
    stack[0].last_face = NO_FACE;

    *next_bound = UINT8_MAX;

    ++(*nodes);

    while (1) {
        frame_t *current = &stack[depth];
        // Calculate the F-score = g + h
        uint8_t f = depth + heuristic(current->p, current->o);

        // Pruning the nodes whose F-score is higher than the bound
        if (f > bound) {

            // Record the pruned nodes whose F-score is smallest
            if (f < *next_bound) *next_bound = f;

            if (depth == 0) return 0;

            --depth;
            continue;
        }

        // Find the solved state
        if (current->p == 0 && current->o == 0) {
            *solution_length = depth;
            return 1;
        }

        // Restrict the max depth
        if (depth == MAX_DEPTH) {
            if (depth == 0) return 0;

            --depth;
            continue;
        }

        // All probable moves have been tried, get back to the parent node
        if (stack[depth].next_move == MOVES) {
            if (depth == 0) return 0;

            --depth;
            continue;
        }

        // Try next move
        uint8_t move = stack[depth].next_move++;
        uint8_t face = move_faces[move];

        // Ssame-face pruning
        if (face == stack[depth].last_face) continue;

        path[depth] = move;
        uint16_t child_p;
        uint16_t child_o;
        
        // 3-quarter-turn transition table
        // apply_ranked_move(stack[depth].p, stack[depth].o, move, &child_p, &child_o);

        // Direct move transition table
        child_p = permutation_direct_move_transition[move][stack[depth].p];
        child_o = orientation_direct_move_transition[move][stack[depth].o];

        ++depth;

        stack[depth].p = child_p;
        stack[depth].o = child_o;
        stack[depth].next_move = 0;
        stack[depth].last_face = face;

        ++(*nodes);
    }
}

static int ida_star(uint16_t start_p, uint16_t start_o, uint8_t *path, uint8_t *solution_length, uint64_t *total_nodes)
{

    uint8_t bound = heuristic(start_p, start_o);

    *total_nodes = 0;

    while (bound <= MAX_DEPTH) {
        uint8_t next_bound;
        uint64_t iteration_nodes = 0;

        int found = ida_iteration(start_p, start_o, bound, path, solution_length, &next_bound, &iteration_nodes);

        *total_nodes += iteration_nodes;

        if (found) return 1;

        // Can't find
        if (next_bound == UINT8_MAX)
            return 0;

        bound = next_bound;
    }

    return 0;
}

static uint8_t heuristic_no_transition(const state_t *state)
{
    uint32_t rank = rank_state(state);

    uint16_t p = (uint16_t)(rank / ORIENTATIONS);
    uint16_t o = (uint16_t)(rank % ORIENTATIONS);

    uint8_t hp = permutation_pdb[p];
    uint8_t ho = orientation_pdb[o];

    return hp > ho ? hp : ho;
}

static int ida_no_transition_iteration(state_t start, uint8_t bound, uint8_t *path, uint8_t *solution_length, uint8_t *next_bound, uint64_t *nodes)
{
    frame_t stack[MAX_DEPTH + 1];
    uint8_t depth = 0;

    stack[0].state = start;
    stack[0].next_move = 0;
    stack[0].last_face = NO_FACE;

    *next_bound = UINT8_MAX;

    ++(*nodes);

    while (1) {
        state_t *current = &stack[depth].state;
        // Calculate the F-score = g + h
        uint8_t f = depth + heuristic_no_transition(current);

        // Pruning the nodes whose F-score is higher than the bound
        if (f > bound) {

            // Record the pruned nodes whose F-score is smallest
            if (f < *next_bound) *next_bound = f;

            if (depth == 0) return 0;

            --depth;
            continue;
        }

        // Find the solved state
        if (rank_state(current) == 0) {
            *solution_length = depth;
            return 1;
        }

        // Restrict the max depth
        if (depth == MAX_DEPTH) {
            if (depth == 0) return 0;

            --depth;
            continue;
        }

        // All probable moves have been tried, get back to the parent node
        if (stack[depth].next_move == MOVES) {
            if (depth == 0) return 0;

            --depth;
            continue;
        }

        // Try next move
        uint8_t move = stack[depth].next_move++;
        uint8_t face = move / 3U;

        // Ssame-face pruning
        if (face == stack[depth].last_face) continue;

        path[depth] = move;
        state_t child = apply_move(stack[depth].state, move);

        ++depth;

        stack[depth].state = child;
        stack[depth].next_move = 0;
        stack[depth].last_face = face;

        ++(*nodes);
    }
}

static int ida_star_no_transition(state_t start, uint8_t *path, uint8_t *solution_length, uint64_t *total_nodes)
{

    uint8_t bound = heuristic_no_transition(&start);

    *total_nodes = 0;

    while (bound <= MAX_DEPTH) {
        uint8_t next_bound;
        uint64_t iteration_nodes = 0;

        int found = ida_no_transition_iteration(start, bound, path, solution_length, &next_bound, &iteration_nodes);

        *total_nodes += iteration_nodes;

        if (found) return 1;

        // Can't find
        if (next_bound == UINT8_MAX)
            return 0;

        bound = next_bound;
    }

    return 0;
}

static int is_solved(const state_t *state)
{
    return memcmp(state, &solved, sizeof(*state)) == 0;
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

static int verify_path(state_t start, const uint8_t *path, uint8_t solution_length)
{
    state_t check = start;

    for (uint8_t i = 0; i < solution_length; ++i) {
        check = apply_move(check, path[i]);
    }

    return is_solved(&check);
}

int main(void)
{
    uint8_t *exact_dist = build_exact_distances();

    if (!exact_dist) {
        fprintf(stderr, "could not build exact distance table\n");
        return 1;
    }

    uint32_t failures = 0;
    uint64_t total_nodes = 0;

    for (uint32_t rank = 0; rank < TEST_LIMIT; ++rank) {

        state_t start;
        uint8_t path[MAX_DEPTH];
        uint8_t solution_length;
        uint64_t nodes = 0;
        unrank_state(rank, &start);

        // Factored transition table
        // uint16_t start_p = rank_permutation(&start);
        // uint16_t start_o = rank_orientation(&start);
        
        // int found = ida_star(start_p, start_o, path, &solution_length, &nodes);

        // Full state without transition table
        int found = ida_star_no_transition(start, path, &solution_length, &nodes);

        // Chech ida_star found the path
        if (!found) {
            ++failures;

            printf("search failed at rank %u\n", rank);
            continue;
        }

        // Check the returned path is correct
        if (!verify_path(start, path, solution_length)) {
            ++failures;

            printf("invalid path at rank %u\n", rank);

            continue;
        }

        // Chechk ida_star returns the optimal path
        uint8_t expected = exact_dist[rank];

        if (solution_length != expected) {
            ++failures;

            printf("distance mismatch at rank %u: " "ida=%u bfs=%u\n", rank, solution_length, expected);

            continue;
        }

        total_nodes += nodes;
    }

    printf("checked %u states\n", (unsigned)TEST_LIMIT);

    printf("failures: %u\n", failures);

    printf("total IDA* nodes: %llu\n", (unsigned long long)total_nodes);

    free(exact_dist);

    return failures == 0 ? 0 : 1;
}