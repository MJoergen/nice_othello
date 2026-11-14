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
