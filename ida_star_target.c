#include <stdint.h>
#include "utils/pdb_tables.h"
#include "test_case.h"

enum {
    CUBIES = 7,
    MOVES = 9,
    MAX_DEPTH = 11,
    NO_FACE = 3,
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

static const uint8_t move_faces[MOVES] = {
    0, 0, 0, 1, 1, 1, 2, 2, 2
};

static const uint8_t move_turns[MOVES] = {
    1, 2, 3, 1, 2, 3, 1, 2, 3
};

int main(void)
{
    state_t start;
    uint8_t path[MAX_DEPTH];
    uint8_t solution_length;

    // Validate input_state
    for(int i=0; i < CUBIES; i++) {
        if(input_state[i] < '1' || input_state[i] > '7') return 2;
        start.p[i] = (uint8_t)(input_state[i] - '1');
    }

    for(int i=0; i < CUBIES; i++) {
        if(input_state[CUBIES + i] < '1' || input_state[CUBIES + i] > '3') return 2;
        start.o[i] = (uint8_t)(input_state[CUBIES + i] - '1');
    }

    uint8_t sum = 0;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        for (uint8_t j = 0; j < i; ++j)
            if (start.p[j] == start.p[i])
                return 2;

        sum = (uint8_t) (sum + start.o[i]);
    }
    while(sum >= 3) sum -= 3;

    if (!(input_state[14] == '\0' && sum == 0))  return 2;
        
    // rank permutation
    uint16_t start_p;
    uint8_t smaller[6];
    for (uint8_t i = 0; i < 6; ++i) {
        smaller[i] = 0;
        for (uint8_t j = (uint8_t)(i + 1U); j < CUBIES; ++j) {
            if (start.p[j] < start.p[i])
                ++smaller[i];
        }
    }

    start_p = smaller[0];
    start_p = (start_p << 2) + (start_p << 1) + smaller[1];
    start_p = (start_p << 2) + start_p + smaller[2];
    start_p = (start_p << 2) + smaller[3];
    start_p = (start_p << 1) + start_p + smaller[4];
    start_p = (start_p << 1) + smaller[5];

    // rank orientation
    uint16_t start_o = 0;
    for (uint8_t i = 0; i < 6; ++i)
        start_o = (uint16_t)((start_o << 1U) + start_o + start.o[i]);

    // run IDA* search
    uint8_t bound = permutation_pdb[start_p] > orientation_pdb[start_o] ?
                    permutation_pdb[start_p] : orientation_pdb[start_o];

    frame_t stack[MAX_DEPTH + 1];
    
    uint8_t found = 0;
    while (bound <= MAX_DEPTH) {
        uint8_t depth = 0;
        uint8_t next_bound = UINT8_MAX;
        
        stack[0].p = start_p;
        stack[0].o = start_o;
        stack[0].next_move = 0;
        stack[0].last_face = NO_FACE;

        while (1) {
            frame_t *current = &stack[depth];
            // Calculate the F-score = g + h
            uint8_t hp = permutation_pdb[current->p];
            uint8_t ho = orientation_pdb[current->o];
            uint8_t f = depth + (hp > ho ? hp : ho);

            // Pruning the nodes whose F-score is higher than the bound
            if (f > bound) {

                // Record the pruned nodes whose F-score is smallest
                if (f < next_bound) next_bound = f;

                if (depth == 0) break;

                --depth;
                continue;
            }

            // Find the solved state
            if (current->p == 0 && current->o == 0) {
                solution_length = depth;
                found = 1;
                break;
            }

            // Restrict the max depth
            if (depth == MAX_DEPTH) {
                --depth;
                continue;
            }

            // All probable moves have been tried, get back to the parent node
            if (stack[depth].next_move == MOVES) {
                if (depth == 0) break;

                --depth;
                continue;
            }

            // Try next move
            uint8_t move = stack[depth].next_move++;
            uint8_t face = move_faces[move];

            // Same-face pruning
            if (face == stack[depth].last_face) continue;

            path[depth] = move;
            uint16_t child_p = permutation_direct_move_transition[move][stack[depth].p];
            uint16_t child_o = orientation_direct_move_transition[move][stack[depth].o];

            ++depth;

            stack[depth].p = child_p;
            stack[depth].o = child_o;
            stack[depth].next_move = 0;
            stack[depth].last_face = face;
        }
        if (found) break;
        // Can't find
        if (next_bound == UINT8_MAX)
            return 1;
        
        bound = next_bound;
    }

    if(!found) return 1;
    if (solution_length != expected_length) return 1;
    
    state_t check = start;
    state_t next;

    state_t *current = &check;
    state_t *next_state = &next;
    for(int k = 0; k < solution_length; k++) {
        uint8_t move = path[k];
        uint8_t turns = move_turns[move];
        uint8_t face = move_faces[move];

        for (uint8_t i = 0; i < turns; i++) {
            for (uint8_t j = 0; j < CUBIES; j++) {
                uint8_t from = source[face][j];

                next_state->p[j] = current->p[from];

                uint8_t orientation = current->o[from] + twist[face][j];
                if(orientation >= 3) orientation -= 3;

                next_state->o[j] = orientation;
            }
            state_t *tmp = current;
            current = next_state;
            next_state = tmp;
        }
    }

    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (current->p[i] != i || current->o[i] != 0)
            return 1;
    }

    return 0;
}