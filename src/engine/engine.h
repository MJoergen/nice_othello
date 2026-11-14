/***************************************************************
 * engine.h: the interface of the engine, which the front ends and
 * the tests use. The engine does no I/O, reads no clock, and calls
 * no malloc, so that the same code builds for NICE and the host.
 *
 * The squares are numbered 0 to 63: a1 is 0, h1 is 7, a2 is 8, and
 * h8 is 63. As in Othello's notation, the columns are a to h and the
 * rows 1 to 8, row 1 at the top. A move is a square, or ENGINE_PASS.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>

// The contents of a square, and the sides. Black moves first.
#define ENGINE_EMPTY 0
#define ENGINE_BLACK 1
#define ENGINE_WHITE 2

#define ENGINE_PASS 64        // The move of a side with no legal move
#define ENGINE_NO_MOVE (-1)

// The longest list of moves in a position (33 is the most known),
// with room to spare.
#define ENGINE_MAX_MOVES 64

// The text of a position: 64 characters, a1 to h8 row by row, X a
// black disc, O a white one, and - an empty square; a space; and the
// side to move, X or O. The format of Edax's and NBoard's positions.
#define ENGINE_POS_LEN 66     // Without the ending 0

// A new game, from the start, and forgets what earlier searches
// learned (the transposition table).
void engine_new_game(void);

// Sets up the position of the text s (ENGINE_POS_LEN). It also takes
// lower case, '.' for an empty square, and '*' for a black disc.
// What follows the position, if anything, must start with a space, a
// tab, a newline, or ';', as in the .obf files of Edax. Returns 0, and
// changes nothing, if s is not a position. The history of the game is
// cleared.
int engine_set_position(const char *s);

// Writes the position into buf, ENGINE_POS_LEN + 1 characters.
void engine_get_position(char *buf);

int engine_square(int sq);        // ENGINE_EMPTY, ENGINE_BLACK, ENGINE_WHITE
int engine_side_to_move(void);    // ENGINE_BLACK or ENGINE_WHITE
void engine_count(int *black, int *white);

// The legal moves, in order of the squares, into list. A side with
// no legal move, whose opponent has one, has the move ENGINE_PASS
// alone. Returns the number of moves: 0 at the end of the game.
int engine_legal_moves(int *list);

int engine_game_over(void);       // Whether neither side can move

// Makes a move. Returns 0, and changes nothing, if it is not legal.
int engine_make_move(int move);

// Takes back the last move. Returns 0 if there is none.
int engine_undo(void);

// The move of the text s, such as "d3", "D3", or "pass", or
// ENGINE_NO_MOVE if s names none. It need not be legal.
int engine_parse_move(const char *s);

// The name of a move, such as "d3" or "pass", into buf, 5 characters.
void engine_move_name(int move, char *buf);

// The number of leaves of the tree of moves of depth depth from the
// position. A pass counts as a move. A position at the end of the
// game, before depth is reached, counts as one leaf.
uint64_t engine_perft(int depth);

/***************************************************************
 * The search
 *
 * Scores are for the side to move, in hundredths of a disc. The end
 * of the game scores 100 times the final disc difference, with the
 * empty squares to the winner, as in the rules of the World Othello
 * Federation.
 ***************************************************************/

// The longest principal variation that engine_info_t holds.
#define ENGINE_MAX_PV 24

// What limits a search. A field of 0 sets no limit. A search that a
// limit stops still returns a legal move: the best of the last
// iteration, or of the one it stopped in, once that has searched the
// move that was best before.
typedef struct
{
    int      depth;       // The deepest iteration, in moves
    uint32_t nodes;       // The most nodes to visit
    int    (*stop)(void); // Called every 1024 nodes; nonzero stops the search
    // With this many empty squares or fewer, the search solves the
    // position exactly, after a few iterations to order the moves.
    // Without it, it reaches the end only when an iteration does.
    int      solve;
    int      wld;         // Solve for win, draw, or loss only
} engine_limits_t;

#define ENGINE_INEXACT 0  // The kinds of score of engine_info_t
#define ENGINE_EXACT   1  // The final disc difference
#define ENGINE_WLD     2  // Only its sign: win, draw, or loss

// What the search found in an iteration.
typedef struct
{
    int      depth;
    int      score;
    int      exact;       // ENGINE_INEXACT, ENGINE_EXACT, or ENGINE_WLD
    uint32_t nodes;       // From the start of the search
    int      pv_len;
    int      pv[ENGINE_MAX_PV];  // The principal variation, from the root
} engine_info_t;

// Called by engine_search after each iteration that it completes, and
// after one that a limit cut short, once it has a move, so that the
// last call's pv[0] is the move returned. A nonzero return stops the
// search.
typedef int (*engine_info_fn)(const engine_info_t *info);

// Searches the position by iterative deepening, until a limit is
// reached, and returns the best move: ENGINE_PASS if the side to move
// must pass, and ENGINE_NO_MOVE at the end of the game, without a
// search. info may be NULL. The position is unchanged afterwards.
int engine_search(const engine_limits_t *limits, engine_info_fn info);

// The static evaluation of the position, for the side to move.
int engine_eval(void);

/***************************************************************
 * The evaluation
 ***************************************************************/

#define ENGINE_EVAL_HAND   0   // Written by hand (eval.c), the default
#define ENGINE_EVAL_NTUPLE 1   // The N-tuple network (ntuple.c)

// Sets the evaluation of engine_eval and engine_search, and clears the
// transposition table, whose scores were of the other one.
void engine_set_eval(int kind);

// The weights of the N-tuple evaluation, which the caller may change:
// *n of them, laid out as ntuple.c says.
int16_t *engine_ntuple_weights(int *n);

// The patterns (a bit for each, NT_MASK) and the stages of the build,
// which a file of weights must have.
void engine_ntuple_layout(uint32_t *mask, int *stages);

#endif // ENGINE_H
