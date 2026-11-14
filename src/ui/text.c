/***************************************************************
 * text.c: the text front end, on the console.
 *
 * Commands, one per line:
 *   new             a new game
 *   pos POSITION    set up a position: 64 characters of X, O, and -,
 *                   a1 to h8, a space, and the side to move, X or O
 *   show            the board, and the position as pos takes it
 *   moves           the legal moves
 *   move M, or M    make a move, such as d3, or pass
 *   undo            take back the last move
 *   perft N         count the leaves of the tree of moves of depth N
 *   divide N        perft N - 1 after each legal move
 *   help
 *   quit
 *
 * Black is X and white is O, as in Edax and NBoard. perft prints the
 * time it took, in microseconds of platform_micros: steps in niceemu.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/engine.h"
#include "../platform/platform.h"
#include "version.h"

static const char disc_chars[] = "-XO";

/***************************************************************
 * next_word
 * Returns the next word of *ps, which it ends with a 0, and moves
 * *ps past it. Returns "" at the end of the line.
 ***************************************************************/
static char *next_word(char **ps)
{
    char *s = *ps;
    while (*s == ' ' || *s == '\t')
        s++;
    char *w = s;
    while (*s && *s != ' ' && *s != '\t')
        s++;
    if (*s)
        *s++ = '\0';
    *ps = s;
    return w;
} // end of next_word

/***************************************************************
 * read_line
 * Reads a line of the console into s, of size bytes. A line ends at
 * LF or CR, and an LF right after a CR is the same end. A line longer
 * than s loses the rest. Returns 0 at the end of the input.
 ***************************************************************/
static int read_line(char *s, int size)
{
    static int last;
    int len = 0;

    for (;;)
    {
        int c = platform_getc();
        if (c < 0)
        {
            s[len] = '\0';
            return len > 0;
        }
        int prev = last;
        last = c;
        if (c == '\n' && prev == '\r')
            continue;
        if (c == '\n' || c == '\r')
            break;
        if (len < size - 1)
            s[len++] = (char) c;
    }
    s[len] = '\0';
    return 1;
} // end of read_line

static void print_board(void)
{
    char pos[ENGINE_POS_LEN + 1];
    int black, white;

    printf("  a b c d e f g h\n");
    for (int row = 0; row < 8; ++row)
    {
        printf("%d", row + 1);
        for (int col = 0; col < 8; ++col)
            printf(" %c", disc_chars[engine_square(row * 8 + col)]);
        putchar('\n');
    }
    engine_count(&black, &white);
    printf("X %d, O %d, %c to move\n", black, white, disc_chars[engine_side_to_move()]);
    engine_get_position(pos);
    printf("pos %s\n", pos);
} // end of print_board

static void print_moves(void)
{
    int list[ENGINE_MAX_MOVES];
    char name[5];
    int n = engine_legal_moves(list);

    printf("moves");
    for (int i = 0; i < n; ++i)
    {
        engine_move_name(list[i], name);
        printf(" %s", name);
    }
    printf("\n");
} // end of print_moves

/***************************************************************
 * print_state
 * After a move: the end of the game, with the result, or a pass
 * that the side to move must make.
 ***************************************************************/
static void print_state(void)
{
    int list[ENGINE_MAX_MOVES];
    int n = engine_legal_moves(list);

    if (n == 0)
    {
        int black, white;
        engine_count(&black, &white);
        printf("Game over: X %d, O %d, %s\n", black, white,
               black > white ? "X wins" : white > black ? "O wins" : "a draw");
    }
    else if (list[0] == ENGINE_PASS)
        printf("%c must pass\n", disc_chars[engine_side_to_move()]);
} // end of print_state

static void do_move(const char *s)
{
    int move = engine_parse_move(s);
    if (move == ENGINE_NO_MOVE)
    {
        printf("Invalid move: %s\n", s);
        return;
    }
    if (!engine_make_move(move))
    {
        printf("Illegal move: %s\n", s);
        return;
    }
    print_state();
} // end of do_move

// The depth of perft and divide, 0 to 20.
static int parse_depth(const char *s)
{
    if (!*s)
        return -1;
    for (const char *p = s; *p; ++p)
        if (*p < '0' || *p > '9')
            return -1;
    int d = atoi(s);
    return d <= 20 ? d : -1;
} // end of parse_depth

static void do_perft(const char *arg)
{
    int depth = parse_depth(arg);
    if (depth < 0)
    {
        printf("Invalid depth: %s\n", arg);
        return;
    }
    uint32_t t0 = platform_micros();
    uint64_t n = engine_perft(depth);
    uint32_t t = platform_micros() - t0;
    printf("perft %d: %llu time %lu\n", depth, (unsigned long long) n, (unsigned long) t);
} // end of do_perft

static void do_divide(const char *arg)
{
    int depth = parse_depth(arg);
    if (depth < 1)
    {
        printf("Invalid depth: %s\n", arg);
        return;
    }
    int list[ENGINE_MAX_MOVES];
    char name[5];
    int n = engine_legal_moves(list);
    uint64_t total = 0;

    for (int i = 0; i < n; ++i)
    {
        engine_make_move(list[i]);
        uint64_t count = engine_perft(depth - 1);
        engine_undo();
        engine_move_name(list[i], name);
        printf("%s %llu\n", name, (unsigned long long) count);
        total += count;
    }
    if (n == 0)
        total = 1;    // The end of the game, which perft counts as a leaf
    printf("perft %d: %llu\n", depth, (unsigned long long) total);
} // end of do_divide

static void print_help(void)
{
    printf("Commands:\n"
           "  new             a new game\n"
           "  pos POSITION    set up a position: 64 of X O -, a1 to h8, a space, and X or O to move\n"
           "  show            the board\n"
           "  moves           the legal moves\n"
           "  move M, or M    make a move, such as d3, or pass\n"
           "  undo            take back the last move\n"
           "  perft N         count the leaves of the tree of moves of depth N\n"
           "  divide N        perft N, for each legal move\n"
           "  help\n"
           "  quit\n");
} // end of print_help

int main(void)
{
    static char line[256];

    printf("nice_othello %s\n", nice_othello_version);
    engine_new_game();

    for (;;)
    {
        printf("> ");
        fflush(stdout);
        if (!read_line(line, sizeof line))
            break;

        char *rest = line;
        char *cmd = next_word(&rest);

        if (!*cmd)
            continue;
        else if (!strcmp(cmd, "quit"))
            break;
        else if (!strcmp(cmd, "new"))
            engine_new_game();
        else if (!strcmp(cmd, "pos"))
        {
            while (*rest == ' ' || *rest == '\t')
                rest++;
            if (!engine_set_position(rest))
                printf("Invalid position: %s\n", rest);
            else
                print_state();
        }
        else if (!strcmp(cmd, "show"))
            print_board();
        else if (!strcmp(cmd, "moves"))
            print_moves();
        else if (!strcmp(cmd, "move"))
            do_move(next_word(&rest));
        else if (!strcmp(cmd, "undo"))
        {
            if (!engine_undo())
                printf("Nothing to undo\n");
        }
        else if (!strcmp(cmd, "perft"))
            do_perft(next_word(&rest));
        else if (!strcmp(cmd, "divide"))
            do_divide(next_word(&rest));
        else if (!strcmp(cmd, "help"))
            print_help();
        else if (engine_parse_move(cmd) != ENGINE_NO_MOVE)
            do_move(cmd);
        else
            printf("Unknown command: %s\n", cmd);
    }
    printf("\n");
    return 0;
} // end of main
