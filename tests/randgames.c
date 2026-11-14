/***************************************************************
 * randgames.c: plays games of random moves to their end, and checks
 * at every position the bitboard move generator against the plain
 * one of check.c, the moves through the engine's interface, the
 * indexes of the N-tuple evaluation against those from scratch, and
 * that undo takes the game back to its start, indexes and all.
 *
 * Perft from the start reaches only the first moves of a game; these
 * games reach the edges, the corners, the passes, and full boards.
 *
 * The number of games is the first argument on the host, or
 * RANDGAMES. The moves come from a fixed generator, so every run, on
 * the host and on NICE, plays the same games. It prints the number of
 * positions and passes and the sums of the final discs, which must be
 * the same on both, and main returns the number of failures.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/internal.h"

#ifndef RANDGAMES
#define RANDGAMES 1000
#endif

static uint32_t seed = 12345;

static uint32_t next_random(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 16;
}

// Whether the indexes of the N-tuple evaluation are those of the board
static int indexes_right(void)
{
    int black = board.side == ENGINE_BLACK;
    return nt_consistent(black ? board.own : board.opp, black ? board.opp : board.own);
}

int main(int argc, char **argv)
{
    int games = argc > 1 ? atoi(argv[1]) : RANDGAMES;
    int failures = 0;
    uint32_t positions = 0, passes = 0, black_sum = 0, white_sum = 0;
    char start[ENGINE_POS_LEN + 1], end[ENGINE_POS_LEN + 1];

    for (int g = 0; g < games; ++g)
    {
        engine_new_game();
        engine_get_position(start);
        int len = 0;

        for (;;)
        {
            int list[ENGINE_MAX_MOVES];
            int n = engine_legal_moves(list);
            ++positions;

            if (!moves_consistent(board.own, board.opp))
            {
                engine_get_position(end);
                printf("Moves differ in game %d: %s\n", g, end);
                ++failures;
                break;
            }
            // The list is the bitboard of the plain generator, or a pass
            bb_t moves = 0;
            for (int i = 0; i < n; ++i)
                if (list[i] != ENGINE_PASS)
                    moves |= BB(list[i]);
            int pass = n == 1 && list[0] == ENGINE_PASS;
            if (moves != slow_moves(board.own, board.opp)
                || pass != (!moves && slow_moves(board.opp, board.own) != 0)
                || (n == 0) != engine_game_over())
            {
                engine_get_position(end);
                printf("The list of moves is wrong in game %d: %s\n", g, end);
                ++failures;
                break;
            }
            if (n == 0)
                break;

            // An illegal move is refused, and changes nothing
            bb_t own = board.own, opp = board.opp;
            int occupied = lowest_square(own | opp);
            if (engine_make_move(occupied) || (!pass && engine_make_move(ENGINE_PASS))
                || board.own != own || board.opp != opp)
            {
                printf("An illegal move was made in game %d\n", g);
                ++failures;
                break;
            }

            int move = list[next_random() % n];
            passes += move == ENGINE_PASS;
            bb_t flips = move == ENGINE_PASS ? 0 : slow_flips(own, opp, move);
            engine_make_move(move);
            ++len;
            bb_t placed = move == ENGINE_PASS ? 0 : BB(move);
            if (board.opp != (own | flips | placed) || board.own != (opp & ~flips) || !indexes_right())
            {
                printf("The move went wrong in game %d\n", g);
                ++failures;
                break;
            }
        }

        int black, white;
        engine_count(&black, &white);
        black_sum += black;
        white_sum += white;

        int undo_ok = 1;
        while (len--)
        {
            engine_undo();
            undo_ok &= indexes_right();
        }
        engine_get_position(end);
        if (strcmp(start, end) || engine_undo() || !undo_ok)
        {
            printf("Undo did not go back to the start in game %d\n", g);
            ++failures;
        }
    }

    printf("games %d positions %lu passes %lu black %lu white %lu failures %d\n", games,
           (unsigned long) positions, (unsigned long) passes, (unsigned long) black_sum,
           (unsigned long) white_sum, failures);
    return failures;
}
