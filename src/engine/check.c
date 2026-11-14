/***************************************************************
 * check.c: a plain move generator, which walks from each square
 * in each direction, to test the bitboards of movegen.c against.
 * It shares nothing with movegen.c but the bitboards.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

static const int dcol[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const int drow[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

bb_t slow_flips(bb_t own, bb_t opp, int sq)
{
    bb_t flips = 0;

    if ((own | opp) & BB(sq))
        return 0;
    for (int d = 0; d < 8; ++d)
    {
        bb_t run = 0;
        int col = sq % 8 + dcol[d];
        int row = sq / 8 + drow[d];
        while (col >= 0 && col < 8 && row >= 0 && row < 8 && (opp & BB(row * 8 + col)))
        {
            run |= BB(row * 8 + col);
            col += dcol[d];
            row += drow[d];
        }
        if (col >= 0 && col < 8 && row >= 0 && row < 8 && (own & BB(row * 8 + col)))
            flips |= run;
    }
    return flips;
} // end of slow_flips

bb_t slow_moves(bb_t own, bb_t opp)
{
    bb_t moves = 0;
    for (int sq = 0; sq < 64; ++sq)
        if (slow_flips(own, opp, sq))
            moves |= BB(sq);
    return moves;
} // end of slow_moves

int moves_consistent(bb_t own, bb_t opp)
{
    if (own & opp)
        return 0;
    if (gen_moves(own, opp) != slow_moves(own, opp))
        return 0;
    for (int sq = 0; sq < 64; ++sq)
        if (!((own | opp) & BB(sq)) && gen_flips(own, opp, sq) != slow_flips(own, opp, sq))
            return 0;

    // popcount and lowest_square, against a walk of the bits
    int n = 0, low = -1;
    for (int sq = 0; sq < 64; ++sq)
        if ((own | opp) & BB(sq))
        {
            if (low < 0)
                low = sq;
            ++n;
        }
    return popcount(own | opp) == n && (low < 0 || lowest_square(own | opp) == low);
} // end of moves_consistent
