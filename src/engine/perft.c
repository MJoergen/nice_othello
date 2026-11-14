/***************************************************************
 * perft.c: perft, the count of the leaves of the tree of legal
 * moves, which tests the move generator.
 *
 * A pass counts as a move, and a position at the end of the game
 * counts as a leaf wherever it is in the tree: the counts of Edax and
 * of the On-Line Encyclopedia of Integer Sequences, A124004.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

/***************************************************************
 * perft
 * At depth 1 it counts the moves, without making them. With
 * CHECK_STATE, it checks the moves and the flips of every node
 * against the plain move generator of check.c.
 ***************************************************************/
static uint64_t perft(bb_t own, bb_t opp, int depth)
{
#ifdef CHECK_STATE
    assert(moves_consistent(own, opp));
#endif
    bb_t moves = gen_moves(own, opp);

    if (!moves)
    {
        if (!gen_moves(opp, own))
            return 1;                    // The end of the game
        return depth > 1 ? perft(opp, own, depth - 1) : 1;   // A pass
    }
    if (depth == 1)
        return (uint64_t) popcount(moves);

    uint64_t count = 0;
    while (moves)
    {
        int sq = lowest_square(moves);
        moves &= moves - 1;
        bb_t flips = gen_flips(own, opp, sq);
        count += perft(opp & ~flips, own | flips | BB(sq), depth - 1);
    }
    return count;
} // end of perft

uint64_t engine_perft(int depth)
{
    if (depth <= 0)
        return 1;
    return perft(board.own, board.opp, depth);
} // end of engine_perft
