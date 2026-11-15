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
 *   go [LIMITS]     the engine searches, and makes its move
 *   eval            the static evaluation of the position
 *   evaluation E    use the evaluation E: ntuple, the N-tuple network
 *                   (the default, with the weights of the build), or
 *                   hand, written by hand
 *   weights FILE    load the weights of the N-tuple network, as
 *                   tools/train.c writes them, and use it (not on NICE,
 *                   which has no files)
 *   solve N         solve exactly at N empty squares or fewer
 *   level N         the strength of go without limits: 1 to 5, or 0,
 *                   for the time of platform_move_time (the default)
 *   nboard N        speak the NBoard protocol from then on (below)
 *   perft N         count the leaves of the tree of moves of depth N
 *   divide N        perft N - 1 after each legal move
 *   help
 *   quit
 *
 * The limits of go: depth N, nodes N, movetime MS, and wld, which
 * solves for win, draw, or loss only. A number alone is a depth. With
 * no depth, nodes, or movetime, the search takes platform_move_time,
 * or with a level, searches to the depth of the level, and solves
 * exactly at its number of empty squares or fewer.
 * go prints a line for each iteration of the search:
 *
 *   info depth D [exact|wld] score S nodes N time T pv MOVES
 *
 * The score is in discs, for the side to move, with two decimals, and
 * exact, the final disc difference, as a whole number; for wld, win,
 * draw, or loss. Then bestmove M, the move it makes.
 *
 * nboard switches to the NBoard protocol of Othello GUIs, for the rest
 * of the input. It takes:
 *
 *   set depth N     search to depth N, and solve exactly at N + 8 empty
 *                   squares or fewer, but no more than
 *                   platform_solve_empties: 6 at first
 *   set game GGF    the game, in GGF: its start, BO[...], and its moves
 *   move M[/...]    a move: F5, or PA for a pass
 *   go              answers "=== M/EVAL/TIME": the engine's move, the
 *                   score in discs for its side, and the time in seconds
 *   hint N          answers "search M EVAL 0 DEPTH" for the best move,
 *                   then "status"
 *   ping N          answers "pong N"
 *   learn           answers "learned"
 *   quit
 *
 * and ignores the rest, such as set contempt and analyze. It answers
 * nboard with "set myname nice_othello", and says what it does in
 * "status" lines.
 *
 * Black is X and white is O, as in Edax and NBoard. The times are in
 * microseconds of platform_micros: steps in niceemu.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/engine.h"
#include "../platform/platform.h"
#include "levels.h"
#include "version.h"
#include "weights_file.h"

static const char disc_chars[] = "-XO";

static int solve_empties;     // Of solve N, platform_solve_empties at first
static int level;             // Of level N (levels.h): 0, none, at first

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

// A score in discs, with two decimals: +1.25, -0.50.
static void print_score(int score)
{
    int a = score < 0 ? -score : score;
    printf("%c%d.%02d", score < 0 ? '-' : '+', a / 100, a % 100);
} // end of print_score

static uint32_t search_start, search_time;

static int stop_search(void)
{
    return search_time && platform_micros() - search_start >= search_time;
} // end of stop_search

static int print_info(const engine_info_t *info)
{
    char name[5];
    printf("info depth %d", info->depth);
    if (info->exact == ENGINE_EXACT)
        printf(" exact score %+d", info->score / 100);
    else if (info->exact == ENGINE_WLD)
        printf(" wld score %s", info->score > 0 ? "win" : info->score < 0 ? "loss" : "draw");
    else
    {
        printf(" score ");
        print_score(info->score);
    }
    printf(" nodes %lu time %lu pv", (unsigned long) info->nodes,
           (unsigned long) (platform_micros() - search_start));
    for (int i = 0; i < info->pv_len; ++i)
    {
        engine_move_name(info->pv[i], name);
        printf(" %s", name);
    }
    printf("\n");
    fflush(stdout);
    return 0;
} // end of print_info

// A number of go or solve: digits only.
static int parse_number(const char *s, uint32_t *n)
{
    if (!*s)
        return 0;
    uint32_t v = 0;
    for (; *s; ++s)
    {
        if (*s < '0' || *s > '9' || v > 400000000u)
            return 0;
        v = v * 10 + (uint32_t) (*s - '0');
    }
    *n = v;
    return 1;
} // end of parse_number

static void do_go(char *rest)
{
    engine_limits_t limits = { 0, 0, stop_search, solve_empties, 0 };
    uint32_t n;
    search_time = 0;

    for (char *w = next_word(&rest); *w; w = next_word(&rest))
    {
        if (!strcmp(w, "wld"))
            limits.wld = 1;
        else if (!strcmp(w, "depth") && parse_number(next_word(&rest), &n) && n <= 64)
            limits.depth = (int) n;
        else if (!strcmp(w, "nodes") && parse_number(next_word(&rest), &n))
            limits.nodes = n;
        else if (!strcmp(w, "movetime") && parse_number(next_word(&rest), &n) && n <= 4000000)
            search_time = n * 1000;
        else if (parse_number(w, &n) && n <= 64)
            limits.depth = (int) n;
        else
        {
            printf("Invalid limit: %s\n", w);
            return;
        }
    }
    if (!limits.depth && !limits.nodes && !search_time && level)
        level_limits(level, &limits);
    else if (!limits.depth && !limits.nodes && !search_time)
        search_time = platform_move_time();

    search_start = platform_micros();
    int move = engine_search(&limits, print_info);
    if (move == ENGINE_NO_MOVE)
    {
        print_state();
        return;
    }
    char name[5];
    engine_move_name(move, name);
    printf("bestmove %s\n", name);
    engine_make_move(move);
    print_state();
} // end of do_go

static void do_weights(const char *name)
{
    weights_header_t h;
    uint32_t mask;
    int stages, n;
    int16_t *w = engine_ntuple_weights(&n);
    engine_ntuple_layout(&mask, &stages);

    if (platform_read_file(name, 0, &h, sizeof h) != (int) sizeof h
        || memcmp(h.magic, WEIGHTS_MAGIC, sizeof h.magic))
    {
        printf("Cannot read weights from %s\n", name);
        return;
    }
    if (h.mask != mask || h.stages != (uint32_t) stages || h.count != (uint32_t) n)
    {
        printf("Weights of patterns 0x%lX in %lu stages, not 0x%lX in %d\n", (unsigned long) h.mask,
               (unsigned long) h.stages, (unsigned long) mask, stages);
        return;
    }
    if (platform_read_file(name, sizeof h, w, (uint32_t) n * 2) != n * 2)
    {
        printf("Cannot read weights from %s\n", name);
        return;
    }
    engine_set_eval(ENGINE_EVAL_NTUPLE);    // Clears the table, of the old weights
    printf("Weights of %s\n", name);
} // end of do_weights

/***************************************************************
 * The NBoard protocol
 ***************************************************************/
static int nboard_depth = 6;

// Sets the game of the GGF text s: the position of BO[8 ...], and the
// moves B[...] and W[...] after it. Returns 0 if it is not a game.
static int nboard_set_game(const char *s)
{
    char pos[ENGINE_POS_LEN + 1];
    const char *b = strstr(s, "BO[8");
    if (!b)
        return 0;
    b += 4;
    int n = 0;
    // The 64 squares, then the side to move: * or X black, O white.
    for (; *b && *b != ']' && n < ENGINE_POS_LEN; ++b)
    {
        char c = *b;
        if (c == '*' || c == 'X' || c == 'x')
            pos[n++] = 'X';
        else if (c == 'O' || c == 'o')
            pos[n++] = 'O';
        else if (c == '-' || c == '.')
            pos[n++] = '-';
        if (n == 64)
            pos[n++] = ' ';
    }
    pos[n] = '\0';
    if (n != ENGINE_POS_LEN || !engine_set_position(pos))
        return 0;
    for (const char *m = b; *m; ++m)
    {
        if ((*m != 'B' && *m != 'W') || m[1] != '[')
            continue;
        char name[5];
        int k = 0;
        for (m += 2; *m && *m != ']' && *m != '/' && k < 4; ++m)
            name[k++] = *m;
        name[k] = '\0';
        int move = (name[0] == 'P' || name[0] == 'p') && (name[1] == 'A' || name[1] == 'a')
                   ? ENGINE_PASS : engine_parse_move(name);
        if (move == ENGINE_NO_MOVE || !engine_make_move(move))
            return 0;
        if (!*m)
            break;
    }
    return 1;
}

// A move as NBoard writes it: F5, or PA.
static void nboard_move_name(int move, char *buf)
{
    if (move == ENGINE_PASS)
    {
        buf[0] = 'P', buf[1] = 'A', buf[2] = '\0';
        return;
    }
    engine_move_name(move, buf);
    buf[0] = (char) (buf[0] - 'a' + 'A');
}

static engine_info_t nboard_info;

static int nboard_on_info(const engine_info_t *info)
{
    nboard_info = *info;
    return 0;
}

// Searches the position at nboard_depth, into nboard_info. Returns the
// move, or ENGINE_NO_MOVE at the end of the game.
static int nboard_search(void)
{
    int solve = nboard_depth + 8;
    if (solve > platform_solve_empties())
        solve = platform_solve_empties();
    engine_limits_t limits = { nboard_depth, 0, NULL, solve, 0 };
    int black, white;
    engine_count(&black, &white);
    if (64 - black - white <= solve)
        limits.depth = 64 - black - white;
    nboard_info.pv_len = 0;
    nboard_info.score = 0;
    nboard_info.depth = 0;
    return engine_search(&limits, nboard_on_info);
}

// The score of nboard_info in discs, with two decimals.
static void print_nboard_score(void)
{
    int s = nboard_info.score;
    printf("%s%d.%02d", s < 0 ? "-" : "", (s < 0 ? -s : s) / 100, (s < 0 ? -s : s) % 100);
}

static void nboard(void)
{
    static char line[2048];
    printf("set myname nice_othello\n");
    fflush(stdout);
    while (read_line(line, sizeof line))
    {
        char *rest = line;
        char *cmd = next_word(&rest);
        if (!strcmp(cmd, "quit"))
            break;
        else if (!strcmp(cmd, "ping"))
            printf("pong %s\n", next_word(&rest));
        else if (!strcmp(cmd, "learn"))
            printf("learned\n");
        else if (!strcmp(cmd, "set"))
        {
            char *what = next_word(&rest);
            uint32_t n;
            if (!strcmp(what, "depth") && parse_number(next_word(&rest), &n) && n >= 1 && n <= 60)
                nboard_depth = (int) n;
            else if (!strcmp(what, "game") && !nboard_set_game(rest))
                printf("status Invalid game\n");
        }
        else if (!strcmp(cmd, "move"))
        {
            char *m = next_word(&rest);
            char *slash = strchr(m, '/');
            if (slash)
                *slash = '\0';
            int move = !strcmp(m, "PA") || !strcmp(m, "pa") ? ENGINE_PASS : engine_parse_move(m);
            if (move == ENGINE_NO_MOVE || !engine_make_move(move))
                printf("status Invalid move %s\n", m);
        }
        else if (!strcmp(cmd, "go") || !strcmp(cmd, "hint"))
        {
            printf("status Thinking\n");
            fflush(stdout);
            uint32_t start = platform_micros();
            int move = nboard_search();
            uint32_t micros = platform_micros() - start;
            char name[5];
            if (move == ENGINE_NO_MOVE)
                printf("status The game is over\n");
            else if (!strcmp(cmd, "go"))
            {
                nboard_move_name(move, name);
                printf("=== %s/", name);
                print_nboard_score();
                printf("/%u.%03u\n", (unsigned) (micros / 1000000),
                       (unsigned) (micros / 1000 % 1000));
                engine_make_move(move);
            }
            else
            {
                nboard_move_name(move, name);
                printf("search %s ", name);
                print_nboard_score();
                printf(" 0 %d\n", nboard_info.depth);
            }
            printf("status\n");
        }
        fflush(stdout);
    }
} // end of nboard

static void print_help(void)
{
    printf("Commands:\n"
           "  new             a new game\n"
           "  pos POSITION    set up a position: 64 of X O -, a1 to h8, a space, and X or O to move\n"
           "  show            the board\n"
           "  moves           the legal moves\n"
           "  move M, or M    make a move, such as d3, or pass\n"
           "  undo            take back the last move\n"
           "  go [LIMITS]     search and make a move: depth N, nodes N, movetime MS, wld\n"
           "  eval            the static evaluation\n"
           "  evaluation E    use the evaluation E: hand or ntuple\n"
           "  weights FILE    load the weights of the N-tuple network (and use it)\n"
           "  solve N         solve exactly at N empty squares or fewer\n"
           "  level N         the strength of go without limits: 1 to 5, or 0 for the time of a move\n"
           "  nboard N        speak the NBoard protocol from then on\n"
           "  perft N         count the leaves of the tree of moves of depth N\n"
           "  divide N        perft N, for each legal move\n"
           "  help\n"
           "  quit\n");
} // end of print_help

int main(void)
{
    static char line[2048];

    printf("nice_othello %s\n", nice_othello_version);
    engine_new_game();
    solve_empties = platform_solve_empties();

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
        else if (!strcmp(cmd, "go"))
            do_go(rest);
        else if (!strcmp(cmd, "eval"))
        {
            printf("eval ");
            print_score(engine_eval());
            printf("\n");
        }
        else if (!strcmp(cmd, "evaluation"))
        {
            char *e = next_word(&rest);
            if (!strcmp(e, "hand"))
                engine_set_eval(ENGINE_EVAL_HAND);
            else if (!strcmp(e, "ntuple"))
                engine_set_eval(ENGINE_EVAL_NTUPLE);
            else
                printf("Invalid evaluation: %s\n", e);
        }
        else if (!strcmp(cmd, "weights"))
            do_weights(next_word(&rest));
        else if (!strcmp(cmd, "solve"))
        {
            uint32_t n;
            if (parse_number(next_word(&rest), &n) && n <= 64)
                solve_empties = (int) n;
            else
                printf("Invalid number of empty squares\n");
        }
        else if (!strcmp(cmd, "level"))
        {
            uint32_t n;
            if (parse_number(next_word(&rest), &n) && n <= LEVELS)
                level = (int) n;
            else
                printf("Invalid level\n");
        }
        else if (!strcmp(cmd, "nboard"))
        {
            nboard();
            break;
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
