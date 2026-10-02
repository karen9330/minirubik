#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "utils/pdb_tables.h"

#define SHOW_RESULT 1
#if SHOW_RESULT
#define PRINT_RES(...) printf(__VA_ARGS__)
#else
#define PRINT_RES(...) ((void)0)
#endif

enum {
    CUBIES = 7,
    MOVES = 9,
    MAX_DEPTH = 11,
    NO_FACE = 3,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
};

typedef struct {
    uint8_t p[CUBIES];
    uint8_t o[CUBIES];
} state_t;

typedef struct {
    uint16_t p;
    uint16_t o;

    /* Next move to try from this node: 0 ... 8 */
    uint8_t next_move;
    /* Face used to reach this node: 0=R, 1=B, 2=D, 3=none */
    uint8_t last_face;
} frame_t;

static const char *const move_names[MOVES] = {
    "R", "R2", "R'",
    "B", "B2", "B'",
    "D", "D2", "D'"
};

/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},  /* R */
    {0, 1, 2, 4, 5, 6, 3},  /* B */
    {0, 2, 5, 3, 1, 4, 6},  /* D */
};

static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static const state_t solved = {
    {0, 1, 2, 3, 4, 5, 6},
    {0, 0, 0, 0, 0, 0, 0}
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

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];

        result.p[i] = state.p[from];
        result.o[i] =
            (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }

    return result;
}

static void apply_ranked_move(uint16_t p, uint16_t o, uint8_t move, uint16_t *next_p, uint16_t *next_o)
{
    uint8_t turns = (uint8_t)(move % 3U + 1U);
    uint8_t face = (uint8_t)(move / 3U);

    *next_p = p;
    *next_o = o;

    for (uint8_t i = 0; i < turns; ++i) {
        *next_p = permutation_transition[face][*next_p];
        *next_o = orientation_transition[face][*next_o];
    }
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t)(move % 3U + 1U);
    uint8_t face = (uint8_t)(move / 3U);

    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, face);

    return state;
}

static int is_solved(const state_t *state)
{
    return memcmp(state, &solved, sizeof(*state)) == 0;
}

static int valid(const state_t *state)
{
    uint8_t sum = 0;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;

        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;

        sum = (uint8_t) (sum + state->o[i]);
    }

    return sum % 3U == 0;
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;

        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;

        (i < 7 ? state->p : state->o)[i % 7] =
            (uint8_t) (input[i] - '1');
    }

    return input[14] == '\0' && valid(state);
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
        uint8_t face = move / 3U;

        // Ssame-face pruning
        if (face == stack[depth].last_face) continue;

        path[depth] = move;
        uint16_t child_p;
        uint16_t child_o;
        apply_ranked_move(stack[depth].p, stack[depth].o, move, &child_p, &child_o);

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
        PRINT_RES("bound %u: %llu nodes%s\n", bound, (unsigned long long) iteration_nodes, found ? " (solution found)" : "");
        *total_nodes += iteration_nodes;

        if (found) return 1;

        // Can't find
        if (next_bound == UINT8_MAX)
            return 0;

        bound = next_bound;
    }

    return 0;
}

static int self_test(uint8_t solution_length, uint8_t *path, state_t start)
{
    state_t check = start;
    for(int i = 0; i < solution_length; i++) check = apply_move(check, path[i]);

    return is_solved(&check);
}

int main(int argc, char **argv)
{
    state_t start;

    if (argc != 2 || !parse_state(argv[1], &start)) {
        fprintf(stderr,
                "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 ? argv[0] : "iddfs");
        return 2;
    }

    uint8_t path[MAX_DEPTH];
    uint8_t solution_length;
    uint64_t total_nodes;

    uint16_t start_p = rank_permutation(&start);
    uint16_t start_o = rank_orientation(&start);
    if (!ida_star(start_p, start_o, path, &solution_length, &total_nodes)) {
        fprintf(stderr, "solution not found within depth %d\n", MAX_DEPTH);
        return 1;
    }

    if(!self_test(solution_length, path, start)) {
        fprintf(stderr, "invalid solution\n");
        return 1;
    }

    for (uint8_t i = 0; i < solution_length; ++i) {
        if (i) putchar(' ');
        fputs(move_names[path[i]], stdout);
    }

    putchar('\n');

    fprintf(stderr, "solution length: %u\n" "total nodes visited: %llu\n",solution_length, (unsigned long long) total_nodes);

    return 0;
}