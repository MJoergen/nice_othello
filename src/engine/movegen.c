/***************************************************************
 * movegen.c: the legal moves, and the discs a move flips, by
 * shifts and masks of the bitboards.
 *
 * A step in a direction is a shift of the bitboard: by 1 to the next
 * column, by 8 to the next row, and by 7 and 9 along the diagonals.
 * A step that leaves the board at the side wraps to the other side
 * of the row, so the steps to the east take away column a after the
 * shift, and those to the west column h. The shifts are by constants,
 * which the translator writes inline for NICE's two words of a
 * uint64_t, so each direction is written out by a macro.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

#define NOT_A 0xFEFEFEFEFEFEFEFEull   // All but column a
#define NOT_H 0x7F7F7F7F7F7F7F7Full   // All but column h
#define INNER 0x7E7E7E7E7E7E7E7Eull   // Columns b to g

/***************************************************************
 * gen_moves
 * In each direction, the discs of opp next to one of own, then those
 * next to them, and so on, at most 6 in a row: then the empty square
 * after them is a move. Along a row or a diagonal only the discs of
 * opp in columns b to g count, so a run never wraps: one in column a
 * or h has nothing after it on the board.
 ***************************************************************/
#define MOVES_UP(S, M)                \
    do {                              \
        bb_t f = (M) & (own << (S));  \
        f |= (M) & (f << (S));        \
        f |= (M) & (f << (S));        \
        f |= (M) & (f << (S));        \
        f |= (M) & (f << (S));        \
        f |= (M) & (f << (S));        \
        moves |= f << (S);            \
    } while (0)

#define MOVES_DOWN(S, M)              \
    do {                              \
        bb_t f = (M) & (own >> (S));  \
        f |= (M) & (f >> (S));        \
        f |= (M) & (f >> (S));        \
        f |= (M) & (f >> (S));        \
        f |= (M) & (f >> (S));        \
        f |= (M) & (f >> (S));        \
        moves |= f >> (S);            \
    } while (0)

bb_t gen_moves(bb_t own, bb_t opp)
{
    bb_t inner = opp & INNER;
    bb_t moves = 0;

    MOVES_UP(1, inner);
    MOVES_DOWN(1, inner);
    MOVES_UP(8, opp);
    MOVES_DOWN(8, opp);
    MOVES_UP(7, inner);
    MOVES_DOWN(7, inner);
    MOVES_UP(9, inner);
    MOVES_DOWN(9, inner);

    return moves & ~(own | opp);
} // end of gen_moves

/***************************************************************
 * gen_flips
 * In each direction from sq, the run of opp's discs, which flip if
 * a disc of own ends it.
 ***************************************************************/
#define FLIP_DIR(OP, S, M)                      \
    do {                                        \
        bb_t f = 0;                             \
        bb_t x = (move OP (S)) & (M);           \
        while (x & opp)                         \
        {                                       \
            f |= x;                             \
            x = (x OP (S)) & (M);               \
        }                                       \
        if (x & own)                            \
            flips |= f;                         \
    } while (0)

bb_t gen_flips(bb_t own, bb_t opp, int sq)
{
    bb_t move = BB(sq);
    bb_t flips = 0;

    FLIP_DIR(<<, 1, NOT_A);
    FLIP_DIR(>>, 1, NOT_H);
    FLIP_DIR(<<, 8, ~0ull);
    FLIP_DIR(>>, 8, ~0ull);
    FLIP_DIR(<<, 9, NOT_A);
    FLIP_DIR(>>, 9, NOT_H);
    FLIP_DIR(<<, 7, NOT_H);
    FLIP_DIR(>>, 7, NOT_A);

    return flips;
} // end of gen_flips

/***************************************************************
 * popcount
 * By halves, as NICE's words are 32 bits: the counts of pairs of
 * bits, then of 4, then of 8, added up.
 ***************************************************************/
static int popcount32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    x += x >> 8;
    x += x >> 16;
    return (int) (x & 0x3F);
} // end of popcount32

int popcount(bb_t b)
{
    return popcount32((uint32_t) b) + popcount32((uint32_t) (b >> 32));
} // end of popcount

int lowest_square(bb_t b)
{
    assert(b);
    return __builtin_ctzll(b);
} // end of lowest_square
