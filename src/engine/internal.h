/***************************************************************
 * internal.h: what the files of the engine share, and the tests
 * that look inside it.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#ifndef INTERNAL_H
#define INTERNAL_H

#include <assert.h>
#include <stdint.h>
#include "engine.h"

typedef uint64_t bb_t;   // A bitboard: bit n is square n

#define BB(sq) ((bb_t) 1 << (sq))

/***************************************************************
 * The position, from the side to move: own its discs, opp its
 * opponent's. side is ENGINE_BLACK or ENGINE_WHITE, the side of own.
 ***************************************************************/
typedef struct
{
    bb_t own;
    bb_t opp;
    int side;
} board_t;

extern board_t board;

// The game, for undo: the positions before each move. Two passes in a
// row end the game, so a game has at most 60 moves and 60 passes.
#define HISTORY_MAX 128
extern board_t history[HISTORY_MAX];
extern int history_len;

/***************************************************************
 * movegen.c
 ***************************************************************/

// The squares where own may move, against opp.
bb_t gen_moves(bb_t own, bb_t opp);

// The discs of opp that own's disc on sq flips, 0 if the move is not
// legal. sq must be empty.
bb_t gen_flips(bb_t own, bb_t opp, int sq);

// The number of bits set in b.
int popcount(bb_t b);

// The lowest square of b, which must not be 0.
int lowest_square(bb_t b);

/***************************************************************
 * check.c: a plain move generator, square by square and direction
 * by direction, for the tests of the one of movegen.c. The host
 * builds with CHECK_STATE use it in perft at every node.
 ***************************************************************/
bb_t slow_moves(bb_t own, bb_t opp);
bb_t slow_flips(bb_t own, bb_t opp, int sq);

// Whether gen_moves and gen_flips agree with the slow ones in the
// position, for every move.
int moves_consistent(bb_t own, bb_t opp);

/***************************************************************
 * eval.c
 ***************************************************************/

// The value of the position for own, to move, in hundredths of a
// disc. Within -EVAL_MAX to EVAL_MAX.
#define EVAL_MAX 6000
int eval_static(bb_t own, bb_t opp);

// The disc difference at the end of the game, for own, with the
// empty squares to the winner.
int final_score(bb_t own, bb_t opp);

/***************************************************************
 * ntuple.c: the N-tuple evaluation
 ***************************************************************/

// The patterns: bit p of NT_MASK uses nt_defs[p]. NT_STAGES stages
// of the game, by the number of discs, have weights of their own.
#ifndef NT_MASK
#define NT_MASK 0xF98      // row1, row2, and the diagonals
#endif
#ifndef NT_STAGES
#define NT_STAGES 2
#endif

#define NT_PATTERNS     12
#define NT_MAX_SQUARES  10
#define NT_MAX_PLACES   96     // 92 with all the patterns
#define NT_MAX_ENTRIES  704    // 696 with all the patterns

// The weights of a stage: 3^n for each pattern of NT_MASK, and the
// weight of having the move.
#define NT_BIT(p) ((NT_MASK >> (p)) & 1)
#define NT_STAGE_SIZE (NT_BIT(0) * 19683 + (NT_BIT(1) + NT_BIT(2)) * 59049        \
    + (NT_BIT(3) + NT_BIT(4) + NT_BIT(5) + NT_BIT(6) + NT_BIT(7)) * 6561            \
    + NT_BIT(8) * 2187 + NT_BIT(9) * 729 + NT_BIT(10) * 243 + NT_BIT(11) * 81 + 1)

typedef struct
{
    const char *name;
    int n;
    uint8_t squares[NT_MAX_SQUARES];
} nt_def_t;

// The trainer (tools/train.c) runs games in threads, each with indexes
// of its own: it builds with NT_TLS _Thread_local.
#ifndef NT_TLS
#define NT_TLS
#endif

extern const nt_def_t nt_defs[NT_PATTERNS];
extern int nt_places;
extern uint8_t nt_place_pattern[NT_MAX_PLACES];
extern uint8_t nt_place_squares[NT_MAX_PLACES][NT_MAX_SQUARES];
extern uint32_t nt_place_offset[NT_MAX_PLACES];  // Of its table, in a stage
extern NT_TLS uint16_t nt_index[NT_MAX_PLACES];

void nt_init(void);
int nt_place_index(int q, bb_t black, bb_t white);  // From scratch
void nt_compute(bb_t black, bb_t white);            // All of nt_index
int nt_consistent(bb_t black, bb_t white);          // nt_index is right
void nt_play(int sq, bb_t flips, int black);
void nt_unplay(int sq, bb_t flips, int black);
int nt_stage(int discs);
// The value for the side to move, black or not, from nt_index.
int nt_eval(int black, int discs);

// The evaluation in use, ENGINE_EVAL_HAND or ENGINE_EVAL_NTUPLE.
extern int eval_kind;

// The value of the position for own, to move, by the evaluation in
// use. black is whether own is black, and nt_index must be that of
// the position for the N-tuple evaluation.
int evaluate(bb_t own, bb_t opp, int black);

/***************************************************************
 * tt.c: the transposition table, of 1 << TT_BITS entries.
 ***************************************************************/
#ifndef TT_BITS
#define TT_BITS 16
#endif

#define TT_EXACT 0    // The kinds of bound of an entry
#define TT_LOWER 1
#define TT_UPPER 2

// The depth of an entry that the endgame solver stored: exact, so it
// holds at any depth.
#define TT_SOLVED 63

#define TT_NO_MOVE 0xFF

typedef struct
{
    int depth;
    int bound;
    int score;
    int move;     // A square, ENGINE_PASS, or TT_NO_MOVE
} tt_hit_t;

void tt_clear(void);
// Fills *hit and returns 1 if the table has the position.
int tt_probe(bb_t own, bb_t opp, tt_hit_t *hit);
void tt_store(bb_t own, bb_t opp, int depth, int bound, int score, int move);

/***************************************************************
 * search.c and solve.c
 ***************************************************************/

// Counts a node, and checks the limits. Returns true if the search
// is to stop.
int count_node(void);
extern int search_stopped;

// The moves of a node, with their flips and their scores for the
// order, in a stack that the nodes share.
#define MOVE_STACK_SIZE 1024
extern bb_t    ms_flips[MOVE_STACK_SIZE];
extern int16_t ms_score[MOVE_STACK_SIZE];
extern uint8_t ms_square[MOVE_STACK_SIZE];
extern int     ms_top;

// The value of a square for the order of the moves.
extern const int8_t square_value[64];

// Moves the move of the highest score among those from i to n - 1
// of the stack at base to i.
void pick_best(int base, int i, int n);

// The exact disc difference of the end of the game, for own, by
// alpha-beta between alpha and beta, in discs, fail-soft. empties is
// the number of empty squares.
int solve(bb_t own, bb_t opp, int alpha, int beta, int empties);

#endif // INTERNAL_H
