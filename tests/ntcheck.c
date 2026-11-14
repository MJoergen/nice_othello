/***************************************************************
 * ntcheck.c: tests the N-tuple evaluation of ntuple.c, with weights
 * at random.
 *
 * It prints the places of each pattern of NT_MASK. Then, for positions
 * of random games:
 *   - the indexes the engine keeps are those from scratch;
 *   - the value of the position is that of each of its 8 images under
 *     the symmetries of the board, as all places of a pattern share
 *     one table;
 *   - a search of depth 1 finds the best value of the moves, each
 *     evaluated from scratch, for the side that made it;
 *   - a search of depth 4 leaves the indexes as they were. On the host,
 *     CHECK_STATE checks them at every node.
 *
 * The arguments on the host, or NTCHECK_POSITIONS: the number of
 * positions. The games come from a fixed generator, and so do the
 * weights, so every run, on the host and on NICE, is the same. It
 * prints the sums of the values and of the nodes, and main returns the
 * number of failures.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "../src/engine/internal.h"

#ifndef NTCHECK_POSITIONS
#define NTCHECK_POSITIONS 200
#endif

static uint32_t seed = 2026;

static uint32_t next_random(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 16;
}

// The image of sq under symmetry s, as ntuple.c makes them.
static int transform(int sq, int s)
{
    int r = sq / 8, c = sq % 8, t;
    if (s & 4)
    {
        t = r; r = c; c = t;
    }
    if (s & 2)
        r = 7 - r;
    if (s & 1)
        c = 7 - c;
    return r * 8 + c;
}

static bb_t transform_bb(bb_t b, int s)
{
    bb_t r = 0;
    for (int sq = 0; sq < 64; ++sq)
        if (b & BB(sq))
            r |= BB(transform(sq, s));
    return r;
}

// The value for the side to move, black or not, from scratch.
static int value(bb_t black_bb, bb_t white_bb, int black)
{
    nt_compute(black_bb, white_bb);
    return nt_eval(black, popcount(black_bb | white_bb));
}

static engine_info_t last;

static int record(const engine_info_t *info)
{
    last = *info;
    return 0;
}

int main(int argc, char **argv)
{
    int positions = argc > 1 ? atoi(argv[1]) : NTCHECK_POSITIONS;
    int failures = 0;
    long values = 0;
    unsigned long nodes = 0;

    engine_new_game();   // nt_init
    for (int p = 0; p < NT_PATTERNS; ++p)
    {
        int n = 0;
        for (int q = 0; q < nt_places; ++q)
            n += nt_place_pattern[q] == p;
        if (n)
            printf("pattern %s squares %d places %d\n", nt_defs[p].name, nt_defs[p].n, n);
    }

    int count;
    int16_t *w = engine_ntuple_weights(&count);
    printf("places %d weights %d stages %d\n", nt_places, count, NT_STAGES);
    for (int i = 0; i < count; ++i)
        w[i] = (int16_t) ((int) (next_random() % 601) - 300);
    engine_set_eval(ENGINE_EVAL_NTUPLE);

    for (int done = 0; done < positions; )
    {
        // A random game of 1 to 40 moves, to a position with a move
        engine_new_game();
        int plies = 1 + (int) (next_random() % 40);
        int list[ENGINE_MAX_MOVES];
        for (int i = 0; i < plies; ++i)
        {
            int n = engine_legal_moves(list);
            if (n == 0)
                break;
            engine_make_move(list[next_random() % n]);
        }
        if (engine_legal_moves(list) == 0 || list[0] == ENGINE_PASS)
            continue;
        ++done;

        int black = board.side == ENGINE_BLACK;
        bb_t bb_black = black ? board.own : board.opp;
        bb_t bb_white = black ? board.opp : board.own;
        int ok = nt_consistent(bb_black, bb_white);
        int v = engine_eval();
        values += v;

        // The images
        for (int s = 1; s < 8; ++s)
            ok &= value(transform_bb(bb_black, s), transform_bb(bb_white, s), black) == v;

        // Depth 1: the best of the moves, each from scratch
        int best = -100000;
        bb_t moves = gen_moves(board.own, board.opp);
        while (moves)
        {
            int sq = lowest_square(moves);
            moves &= moves - 1;
            bb_t f = gen_flips(board.own, board.opp, sq);
            bb_t own = board.own | f | BB(sq), opp = board.opp & ~f;
            int c = -value(black ? own : opp, black ? opp : own, !black);
            if (c > best)
                best = c;
        }
        nt_compute(bb_black, bb_white);

        engine_limits_t limits = { 1, 0, 0, 0, 0 };
        engine_search(&limits, record);
        ok &= last.score == best;

        limits.depth = 4;
        engine_search(&limits, record);
        values += last.score;
        nodes += last.nodes;
        ok &= nt_consistent(bb_black, bb_white);

        if (!ok)
        {
            char pos[ENGINE_POS_LEN + 1];
            engine_get_position(pos);
            printf("Position %d: %s failed\n", done, pos);
            ++failures;
        }
    }

    printf("positions %d values %ld nodes %lu failures %d\n", positions, values, nodes, failures);
    return failures;
}
