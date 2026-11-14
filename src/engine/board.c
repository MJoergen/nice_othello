/***************************************************************
 * board.c: the position and the game: setting up, the moves, and
 * undo.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

board_t board;
board_t history[HISTORY_MAX];
int history_len;

// d4 and e5 white, d5 and e4 black; black to move.
#define START_WHITE (BB(27) | BB(36))
#define START_BLACK (BB(28) | BB(35))

void engine_new_game(void)
{
    board.own = START_BLACK;
    board.opp = START_WHITE;
    board.side = ENGINE_BLACK;
    history_len = 0;
    tt_clear();
} // end of engine_new_game

int engine_set_position(const char *s)
{
    bb_t black = 0, white = 0;

    for (int sq = 0; sq < 64; ++sq)
    {
        switch (s[sq])
        {
        case 'X': case 'x': case '*': black |= BB(sq); break;
        case 'O': case 'o':           white |= BB(sq); break;
        case '-': case '.':                            break;
        default: return 0;    // Also the end of a short s
        }
    }
    if (s[64] != ' ')
        return 0;

    int side;
    switch (s[65])
    {
    case 'X': case 'x': case '*': side = ENGINE_BLACK; break;
    case 'O': case 'o':           side = ENGINE_WHITE; break;
    default: return 0;
    }
    if (s[66] != '\0' && s[66] != ' ' && s[66] != '\t' && s[66] != '\n' && s[66] != ';')
        return 0;

    board.side = side;
    board.own = side == ENGINE_BLACK ? black : white;
    board.opp = side == ENGINE_BLACK ? white : black;
    history_len = 0;
    return 1;
} // end of engine_set_position

int engine_square(int sq)
{
    int other = board.side == ENGINE_BLACK ? ENGINE_WHITE : ENGINE_BLACK;
    if (board.own & BB(sq))
        return board.side;
    if (board.opp & BB(sq))
        return other;
    return ENGINE_EMPTY;
} // end of engine_square

void engine_get_position(char *buf)
{
    static const char chars[] = "-XO";
    for (int sq = 0; sq < 64; ++sq)
        buf[sq] = chars[engine_square(sq)];
    buf[64] = ' ';
    buf[65] = chars[board.side];
    buf[66] = '\0';
} // end of engine_get_position

int engine_side_to_move(void)
{
    return board.side;
} // end of engine_side_to_move

void engine_count(int *black, int *white)
{
    int own = popcount(board.own);
    int opp = popcount(board.opp);
    *black = board.side == ENGINE_BLACK ? own : opp;
    *white = board.side == ENGINE_BLACK ? opp : own;
} // end of engine_count

int engine_game_over(void)
{
    return !gen_moves(board.own, board.opp) && !gen_moves(board.opp, board.own);
} // end of engine_game_over

int engine_legal_moves(int *list)
{
    bb_t moves = gen_moves(board.own, board.opp);
    int n = 0;

    if (!moves)
    {
        if (gen_moves(board.opp, board.own))
            list[n++] = ENGINE_PASS;
        return n;
    }
    while (moves)
    {
        list[n++] = lowest_square(moves);
        moves &= moves - 1;
    }
    return n;
} // end of engine_legal_moves

int engine_make_move(int move)
{
    bb_t flips;

    if (move == ENGINE_PASS)
    {
        if (gen_moves(board.own, board.opp) || !gen_moves(board.opp, board.own))
            return 0;
        flips = 0;
    }
    else
    {
        if (move < 0 || move >= 64 || ((board.own | board.opp) & BB(move)))
            return 0;
        flips = gen_flips(board.own, board.opp, move);
        if (!flips)
            return 0;
    }

    assert(history_len < HISTORY_MAX);
    history[history_len++] = board;

    bb_t own = board.own | flips | (move == ENGINE_PASS ? 0 : BB(move));
    board.own = board.opp & ~flips;
    board.opp = own;
    board.side = board.side == ENGINE_BLACK ? ENGINE_WHITE : ENGINE_BLACK;
    return 1;
} // end of engine_make_move

int engine_undo(void)
{
    if (!history_len)
        return 0;
    board = history[--history_len];
    return 1;
} // end of engine_undo

int engine_parse_move(const char *s)
{
    if ((s[0] == 'p' || s[0] == 'P') && (s[1] == 'a' || s[1] == 'A')
        && (s[2] == 's' || s[2] == 'S') && (s[3] == 's' || s[3] == 'S') && !s[4])
        return ENGINE_PASS;

    int col = s[0] >= 'A' && s[0] <= 'H' ? s[0] - 'A' : s[0] - 'a';
    int row = s[1] - '1';
    if (col < 0 || col > 7 || row < 0 || row > 7 || s[2])
        return ENGINE_NO_MOVE;
    return row * 8 + col;
} // end of engine_parse_move

void engine_move_name(int move, char *buf)
{
    if (move == ENGINE_PASS)
    {
        buf[0] = 'p'; buf[1] = 'a'; buf[2] = 's'; buf[3] = 's'; buf[4] = '\0';
        return;
    }
    buf[0] = (char) ('a' + move % 8);
    buf[1] = (char) ('1' + move / 8);
    buf[2] = '\0';
} // end of engine_move_name
