/***************************************************************
 * ffosuite.c: solves the positions of a suite, such as the FFO
 * positions of Edax's problem/fforum-1-19.obf, read from the console.
 *
 * Each line is a position, as engine_set_position takes it, then ";"
 * and the moves with their exact scores, best first, such as
 * "G8:+18; H1:+12;". The score of the search must be that of the
 * first move, and the move it returns must have that score too, as
 * two moves may share it. Lines that start with % are comments.
 *
 * It prints a line for each position, and a summary. main returns
 * the number of failures. The table is cleared before each position,
 * so that each is solved on its own.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/engine.h"

static engine_info_t last;

static int record(const engine_info_t *info)
{
    last = *info;
    return 0;
}

/***************************************************************
 * listed_score
 * The score that the moves of s, after the position, give move, into
 * *score. The first of them is the best, into *best. Returns 0 if s
 * has no moves, or not move.
 ***************************************************************/
static int listed_score(const char *s, int move, int *best, int *score)
{
    int found = 0, first = 1;
    while ((s = strchr(s, ';')) != NULL)
    {
        ++s;
        while (*s == ' ')
            ++s;
        char name[3] = { s[0], s[1], 0 };
        if (s[2] != ':')
            break;
        int v = atoi(s + 3);
        if (first)
            *best = v;
        first = 0;
        if (engine_parse_move(name) == move)
        {
            *score = v;
            found = 1;
        }
    }
    return !first && found;
} // end of listed_score

int main(void)
{
    static char line[512];
    int number = 0, failures = 0;
    unsigned long total = 0;

    while (fgets(line, sizeof line, stdin) != NULL)
    {
        if (line[0] == '%' || line[0] == '\n' || line[0] == '\0')
            continue;
        ++number;
        engine_new_game();
        if (!engine_set_position(line))
        {
            printf("position %d: invalid\n", number);
            ++failures;
            continue;
        }

        engine_limits_t limits = { 0, 0, 0, 64, 0 };
        last.exact = ENGINE_INEXACT;
        int move = engine_search(&limits, record);
        char name[5];
        engine_move_name(move, name);
        int best = 0, score = 0;
        int ok = last.exact == ENGINE_EXACT && listed_score(line, move, &best, &score)
              && last.score == 100 * best && score == best;
        printf("position %d: %s %+d %s\n", number, name, last.score / 100, ok ? "ok" : "FAIL");
        printf("nodes %lu\n", (unsigned long) last.nodes);
        total += last.nodes;
        failures += !ok;
    }

    printf("positions %d failures %d\n", number, failures);
    printf("nodes %lu\n", total);
    return failures;
}
