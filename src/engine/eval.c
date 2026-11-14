/***************************************************************
 * eval.c: the evaluation, written by hand: the baseline that the
 * learned evaluation of the N-tuple network is to beat.
 *
 * In hundredths of a disc, for the side to move:
 *   mobility            the moves of each side
 *   potential mobility  the empty squares next to the other side's
 *                       discs, where moves may come later
 *   corners             the discs on them
 *   X-squares           b2, g2, b7, and g7, next to an empty corner,
 *                       which give the corner away
 *   C-squares           the edge squares next to an empty corner
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

#define W_MOBILITY     70
#define W_POTENTIAL    20
#define W_CORNER      800
#define W_XSQUARE     300
#define W_CSQUARE     100

#define NOT_A 0xFEFEFEFEFEFEFEFEull
#define NOT_H 0x7F7F7F7F7F7F7F7Full

// The squares next to the discs of b, in all 8 directions.
static bb_t neighbours(bb_t b)
{
    bb_t e = (b << 1) & NOT_A;
    bb_t w = (b >> 1) & NOT_H;
    bb_t row = b | e | w;
    return e | w | (row << 8) | (row >> 8);
} // end of neighbours

// The squares of a corner: the corner, its X-square, and its two
// C-squares.
typedef struct
{
    bb_t corner, x, c;
} corner_t;

static const corner_t corners[4] =
{
    { 1ull << 0,  1ull << 9,  (1ull << 1)  | (1ull << 8)  },  // a1
    { 1ull << 7,  1ull << 14, (1ull << 6)  | (1ull << 15) },  // h1
    { 1ull << 56, 1ull << 49, (1ull << 57) | (1ull << 48) },  // a8
    { 1ull << 63, 1ull << 54, (1ull << 62) | (1ull << 55) },  // h8
};

int eval_static(bb_t own, bb_t opp)
{
    bb_t empty = ~(own | opp);
    int score = 0;

    score += W_MOBILITY * (popcount(gen_moves(own, opp)) - popcount(gen_moves(opp, own)));
    score += W_POTENTIAL * (popcount(neighbours(opp) & empty) - popcount(neighbours(own) & empty));

    for (int i = 0; i < 4; ++i)
    {
        const corner_t *k = &corners[i];
        if (own & k->corner)
            score += W_CORNER;
        else if (opp & k->corner)
            score -= W_CORNER;
        else
        {
            if (own & k->x) score -= W_XSQUARE;
            if (opp & k->x) score += W_XSQUARE;
            score -= W_CSQUARE * (popcount(own & k->c) - popcount(opp & k->c));
        }
    }

    if (score > EVAL_MAX)
        score = EVAL_MAX;
    if (score < -EVAL_MAX)
        score = -EVAL_MAX;
    return score;
} // end of eval_static

int final_score(bb_t own, bb_t opp)
{
    int o = popcount(own), p = popcount(opp);
    int empties = 64 - o - p;
    int diff = o - p;
    if (diff > 0)
        diff += empties;
    else if (diff < 0)
        diff -= empties;
    return diff;
} // end of final_score
