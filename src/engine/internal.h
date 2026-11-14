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

#endif // INTERNAL_H
