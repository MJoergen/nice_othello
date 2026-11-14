/***************************************************************
 * solve.c: the endgame solver, which plays the game out to its end
 * and knows the final disc difference exactly.
 *
 * Fail-soft alpha-beta in discs, as a principal variation search.
 * The order of the moves:
 *   - With more than SOLVE_SORT_EMPTIES empty squares: the move of
 *     the transposition table, then fastest first, the move that
 *     leaves the opponent the fewest moves, then by the square.
 *   - With fewer: by parity, first the moves in a quadrant with an
 *     odd number of empty squares, where the side to move may get the
 *     last move.
 * The table is used with SOLVE_TT_EMPTIES empty squares or more. The
 * last empty square is played out without a search.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

#ifndef SOLVE_SORT_EMPTIES
#define SOLVE_SORT_EMPTIES 6
#endif
#ifndef SOLVE_TT_EMPTIES
#define SOLVE_TT_EMPTIES 9
#endif

// The weight of the opponent's mobility in the order, against
// square_value.
#define ORDER_MOBILITY 16
#define ORDER_TT 10000

static const bb_t quadrants[4] =
{
    0x000000000F0F0F0Full, 0x00000000F0F0F0F0ull,
    0x0F0F0F0F00000000ull, 0xF0F0F0F000000000ull
};

/***************************************************************
 * solve_last
 * The last empty square: own plays it if it can, else opp, else the
 * game ends with it empty.
 ***************************************************************/
static int solve_last(bb_t own, bb_t opp)
{
    int sq = lowest_square(~(own | opp));
    int o = popcount(own);
    int p = 63 - o;
    bb_t f = gen_flips(own, opp, sq);
    if (f)
    {
        int n = popcount(f);
        return (o + 1 + n) - (p - n);
    }
    f = gen_flips(opp, own, sq);
    if (f)
    {
        int n = popcount(f);
        return (o - n) - (p + 1 + n);
    }
    return o > p ? o - p + 1 : o - p - 1;   // o + p is 63, so not equal
} // end of solve_last

/***************************************************************
 * solve_parity
 * The moves of a node near the end, the odd quadrants first.
 ***************************************************************/
static int solve_parity(bb_t own, bb_t opp, bb_t moves, int alpha, int beta, int empties)
{
    bb_t empty = ~(own | opp);
    bb_t odd = 0;
    for (int q = 0; q < 4; ++q)
        if (popcount(empty & quadrants[q]) & 1)
            odd |= quadrants[q];

    int best = -65;
    for (int pass = 0; pass < 2; ++pass)
    {
        bb_t m = moves & (pass ? ~odd : odd);
        while (m)
        {
            int sq = lowest_square(m);
            m &= m - 1;
            bb_t f = gen_flips(own, opp, sq);
            int v = -solve(opp & ~f, own | f | BB(sq), -beta, -alpha, empties - 1);
            if (search_stopped)
                return 0;
            if (v > best)
            {
                best = v;
                if (v > alpha)
                {
                    alpha = v;
                    if (v >= beta)
                        return v;
                }
            }
        }
    }
    return best;
} // end of solve_parity

int solve(bb_t own, bb_t opp, int alpha, int beta, int empties)
{
    if (count_node())
        return 0;
    if (empties == 0)
        return 2 * popcount(own) - 64;
    if (empties == 1)
        return solve_last(own, opp);

    bb_t moves = gen_moves(own, opp);
    if (!moves)
    {
        if (!gen_moves(opp, own))
            return final_score(own, opp);
        return -solve(opp, own, -beta, -alpha, empties);
    }

    if (empties <= SOLVE_SORT_EMPTIES || ms_top + 32 > MOVE_STACK_SIZE)
        return solve_parity(own, opp, moves, alpha, beta, empties);

    // The table: its scores are in hundredths of a disc
    int tt_move = TT_NO_MOVE;
    int use_tt = empties >= SOLVE_TT_EMPTIES;
    tt_hit_t hit;
    if (use_tt && tt_probe(own, opp, &hit))
    {
        tt_move = hit.move;
        if (hit.depth == TT_SOLVED)
        {
            int s = hit.score / 100;
            if (hit.bound == TT_EXACT)
                return s;
            if (hit.bound == TT_LOWER && s > alpha)
                alpha = s;
            if (hit.bound == TT_UPPER && s < beta)
                beta = s;
            if (alpha >= beta)
                return s;
        }
    }

    // The moves, fastest first
    int base = ms_top;
    int n = 0;
    while (moves)
    {
        int sq = lowest_square(moves);
        moves &= moves - 1;
        bb_t f = gen_flips(own, opp, sq);
        ms_square[base + n] = (uint8_t) sq;
        ms_flips[base + n] = f;
        ms_score[base + n] = (int16_t) (sq == tt_move ? ORDER_TT
            : square_value[sq] - ORDER_MOBILITY * popcount(gen_moves(opp & ~f, own | f | BB(sq))));
        ++n;
    }
    ms_top = base + n;

    int a0 = alpha;
    int best = -65, best_move = TT_NO_MOVE;
    for (int i = 0; i < n; ++i)
    {
        pick_best(base, i, n);
        int sq = ms_square[base + i];
        bb_t f = ms_flips[base + i];
        bb_t child_own = opp & ~f, child_opp = own | f | BB(sq);
        int v;
        if (i == 0)
            v = -solve(child_own, child_opp, -beta, -alpha, empties - 1);
        else
        {
            v = -solve(child_own, child_opp, -alpha - 1, -alpha, empties - 1);
            if (v > alpha && v < beta && !search_stopped)
                v = -solve(child_own, child_opp, -beta, -alpha, empties - 1);
        }
        if (search_stopped)
        {
            ms_top = base;
            return 0;
        }
        if (v > best)
        {
            best = v;
            best_move = sq;
            if (v > alpha)
            {
                alpha = v;
                if (v >= beta)
                    break;
            }
        }
    }
    ms_top = base;

    if (use_tt)
        tt_store(own, opp, TT_SOLVED,
                 best <= a0 ? TT_UPPER : best >= beta ? TT_LOWER : TT_EXACT,
                 best * 100, best_move);
    return best;
} // end of solve
