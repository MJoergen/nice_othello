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

void engine_new_game(void);

// Sets up the position of the text s (ENGINE_POS_LEN). It also takes
// lower case, '.' for an empty square, and '*' for a black disc.
// Returns 0, and changes nothing, if s is not a position. The history
// of the game is cleared.
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

#endif // ENGINE_H
