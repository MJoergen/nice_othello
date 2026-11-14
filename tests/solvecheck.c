/***************************************************************
 * solvecheck.c: tests the endgame solver against a plain minimax,
 * with no pruning, table, or order, on positions of random games
 * with a few empty squares left.
 *
 * For each position: the exact score of engine_search is the
 * minimax value, the move it returns has that value, the score of a
 * search for win, draw, or loss only has its sign, and a search with
 * no threshold of solving, whose iterations reach the end by
 * themselves, finds the same value. Positions in which the side to
 * move must pass, or the game is over, are skipped.
 *
 * The arguments on the host, or SOLVECHECK_POSITIONS and
 * SOLVECHECK_EMPTIES: the number of positions and of empty squares.
 * The games come from a fixed generator, so every run, on the host
 * and on NICE, plays the same positions. It prints the sum of the
 * values, and main returns the number of failures.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "../src/engine/internal.h"

#ifndef SOLVECHECK_POSITIONS
#define SOLVECHECK_POSITIONS 200
#endif
#ifndef SOLVECHECK_EMPTIES
#define SOLVECHECK_EMPTIES 10
#endif

static uint32_t seed = 4711;

static uint32_t next_random(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 16;
}

// The minimax value of the end of the game, for own.
static int minimax(bb_t own, bb_t opp)
{
    bb_t moves = gen_moves(own, opp);
    if (!moves)
    {
        if (!gen_moves(opp, own))
            return final_score(own, opp);
        return -minimax(opp, own);
    }
    int best = -65;
    while (moves)
    {
        int sq = lowest_square(moves);
        moves &= moves - 1;
        bb_t f = gen_flips(own, opp, sq);
        int v = -minimax(opp & ~f, own | f | BB(sq));
        if (v > best)
            best = v;
    }
    return best;
}

// The last iteration of a search
static engine_info_t last;

static int record(const engine_info_t *info)
{
    last = *info;
    return 0;
}

int main(int argc, char **argv)
{
    int positions = argc > 1 ? atoi(argv[1]) : SOLVECHECK_POSITIONS;
    int empties = argc > 2 ? atoi(argv[2]) : SOLVECHECK_EMPTIES;
    int failures = 0, done = 0;
    long sum = 0;

    while (done < positions)
    {
        // A random game to empties empty squares
        engine_new_game();
        int list[ENGINE_MAX_MOVES];
        int n;
        while ((n = engine_legal_moves(list)) > 0 && 64 - popcount(board.own | board.opp) > empties)
            engine_make_move(list[next_random() % n]);
        if (n == 0 || list[0] == ENGINE_PASS || 64 - popcount(board.own | board.opp) != empties)
            continue;
        ++done;

        int value = minimax(board.own, board.opp);
        sum += value;

        engine_limits_t limits = { 0, 0, 0, 64, 0 };
        int move = engine_search(&limits, record);
        int score = last.exact == ENGINE_EXACT ? last.score / 100 : 999;
        bb_t f = gen_flips(board.own, board.opp, move);
        int move_value = f ? -minimax(board.opp & ~f, board.own | f | BB(move)) : 999;

        limits.wld = 1;
        engine_search(&limits, record);
        int wld = last.exact == ENGINE_WLD ? last.score : 999;
        int sign = value > 0 ? 100 : value < 0 ? -100 : 0;

        engine_limits_t deepening = { 0, 0, 0, 0, 0 };
        engine_search(&deepening, record);
        int deep = last.exact == ENGINE_EXACT ? last.score / 100 : 999;

        if (score != value || move_value != value || wld != sign || deep != value)
        {
            char pos[ENGINE_POS_LEN + 1];
            engine_get_position(pos);
            printf("Position %d: %s minimax %d, search %d, its move %d, wld %d, deepening %d\n",
                   done, pos, value, score, move_value, wld, deep);
            ++failures;
        }
    }

    printf("positions %d empties %d sum %ld failures %d\n", positions, empties, sum, failures);
    return failures;
}
