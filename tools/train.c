/***************************************************************
 * train.c: learns the weights of the N-tuple evaluation by self-play,
 * with TD(λ) on afterstates. On the host only.
 *
 * Usage: train [OPTIONS] OUTPUT
 *
 * The program plays itself. In each position it makes each legal
 * move, evaluates the position after it (the afterstate), and plays
 * the best for the side to move, or with probability epsilon a move at
 * random. A game starts with 0 to --opening moves at random, which are
 * not learned from. After the game, the value of each afterstate moves
 * towards its λ-return: the final disc difference for the last, and
 * for the others (1 - λ) times the value of the next afterstate plus λ
 * times its λ-return, by the rate alpha.
 *
 * The values are from black's view, in discs, in floating point, as
 * ntuple.c computes them: the weights of the places, plus the weight
 * of having the move, for black, or minus it, for white. Each change
 * of the weight of a state goes, negated, to the weight of the state
 * with the colours swapped, so that the value of a position is minus
 * that of the position with the colours and the side to move swapped.
 *
 * The threads (--threads) play and learn at once, on the same
 * weights, without locks: each its games, with indexes of its own.
 * Each change of a weight is an atomic addition, as with plain
 * additions the threads lose some of each other's changes, and the
 * weights then grow without bound (LOG.md). With one thread and a
 * --seed, a run is the same every time.
 *
 * Every --report games, it plays the learned evaluation against the
 * one written by hand, both choosing their moves by the afterstates
 * alone (a search of 1 ply), from 100 openings of 8 random moves, each
 * played with both colours, and prints the score; and writes OUTPUT:
 * the weights in hundredths of a disc, as 16-bit integers, in the
 * format of weights_file.h, which the text front end loads with
 * "weights FILE". It writes OUTPUT.float too, the weights as they are,
 * which --init reads to go on from them.
 *
 * --selftest trains a few games and checks the antisymmetry of the
 * values, and that the weights of OUTPUT give the values of the floats,
 * to the rounding.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/engine/internal.h"
#include "../src/ui/weights_file.h"

#define NWEIGHTS (NT_STAGES * NT_STAGE_SIZE)
#define MAX_PLIES 128

static float *w;    // NWEIGHTS, in discs

// Adds d to *p, atomically, by a compare and exchange of its bits.
static void atomic_add(float *p, float d)
{
    uint32_t old, new;
    __atomic_load((uint32_t *) p, &old, __ATOMIC_RELAXED);
    do
    {
        float f;
        memcpy(&f, &old, sizeof f);
        f += d;
        memcpy(&new, &f, sizeof new);
    } while (!__atomic_compare_exchange((uint32_t *) p, &old, &new, 1, __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED));
}

// The options
static long   total_games = 1000000;
static int    threads = 1;
static double alpha = 0.0005;
static double lambda = 0.5;
static double epsilon = 0.1;
static int    opening_moves = 6;
static long   report_every = 100000;
static uint64_t seed0 = 1;

static long games_done;     // Atomic: games started

/***************************************************************
 * A generator of random numbers for each thread: xorshift64*.
 ***************************************************************/
typedef struct
{
    uint64_t s;
} rng_t;

static uint32_t rnd(rng_t *r)
{
    r->s ^= r->s >> 12;
    r->s ^= r->s << 25;
    r->s ^= r->s >> 27;
    return (uint32_t) ((r->s * 0x2545F4914F6CDD1Dull) >> 32);
}

static double rnd_unit(rng_t *r)
{
    return rnd(r) / 4294967296.0;
}

/***************************************************************
 * The values, from nt_index
 ***************************************************************/

// For each weight of a stage, but the last, the index in its table of
// the state with the colours swapped: the digits 1 and 2 swapped.
static uint32_t *swapped;

static void init_swapped(void)
{
    swapped = malloc(NT_STAGE_SIZE * sizeof *swapped);
    if (!swapped)
    {
        fprintf(stderr, "train: out of memory\n");
        exit(1);
    }
    for (int q = 0; q < nt_places; ++q)
    {
        int n = nt_defs[nt_place_pattern[q]].n;
        uint32_t size = 1;
        for (int k = 0; k < n; ++k)
            size *= 3;
        for (uint32_t i = 0; i < size; ++i)
        {
            uint32_t index = i, r = 0, p = 1;
            for (int k = 0; k < n; ++k)
            {
                uint32_t d = index % 3;
                index /= 3;
                r += (d == 0 ? 0 : 3 - d) * p;
                p *= 3;
            }
            swapped[nt_place_offset[q] + i] = r;
        }
    }
}

// The value of the position of nt_index for black, with discs discs,
// black to move or not.
static float value(int discs, int black)
{
    const float *ws = w + nt_stage(discs) * NT_STAGE_SIZE;
    float sum = 0;
    for (int q = 0; q < nt_places; ++q)
        sum += ws[nt_place_offset[q] + nt_index[q]];
    float tempo = ws[NT_STAGE_SIZE - 1];
    return black ? sum + tempo : sum - tempo;
}

// Adds delta to the value of the position of index (the indexes of
// its places): to the weight of each place, and the negation to that
// of the state with the colours swapped; and to the weight of the move.
static void update(const uint16_t *index, int discs, int black, float delta)
{
    float *ws = w + nt_stage(discs) * NT_STAGE_SIZE;
    for (int q = 0; q < nt_places; ++q)
    {
        uint32_t i = nt_place_offset[q] + index[q];
        uint32_t j = nt_place_offset[q] + swapped[i];
        if (i == j)
            continue;    // The empty state: 0 by the antisymmetry
        atomic_add(&ws[i], delta);
        atomic_add(&ws[j], -delta);
    }
    atomic_add(&ws[NT_STAGE_SIZE - 1], black ? delta : -delta);
}

/***************************************************************
 * play_game
 * A game of self-play, from the start after up to opening_moves
 * moves at random, and the updates of TD(λ) after it.
 ***************************************************************/
typedef struct
{
    uint16_t index[NT_MAX_PLACES];
    int discs;
    int black;      // Black to move after it
    float v;        // Its value when it was played
} afterstate_t;

static void play_game(rng_t *r)
{
    static _Thread_local afterstate_t as[MAX_PLIES];
    int n_as = 0;
    bb_t black_bb = BB(28) | BB(35), white_bb = BB(27) | BB(36);
    int black = 1;    // To move
    int openings = (int) (rnd(r) % (unsigned) (opening_moves + 1));
    int ply = 0;

    nt_compute(black_bb, white_bb);
    for (;;)
    {
        bb_t own = black ? black_bb : white_bb, opp = black ? white_bb : black_bb;
        bb_t moves = gen_moves(own, opp);
        if (!moves)
        {
            if (!gen_moves(opp, own))
                break;
            black = !black;
            continue;
        }

        int sq = -1;
        int n = popcount(moves);
        float best = 0;
        if (ply < openings || rnd_unit(r) < epsilon)
        {
            int k = (int) (rnd(r) % (unsigned) n);
            bb_t m = moves;
            while (k--)
                m &= m - 1;
            sq = lowest_square(m);
        }
        else
        {
            int discs = popcount(own | opp) + 1;
            for (bb_t m = moves; m; m &= m - 1)
            {
                int s = lowest_square(m);
                bb_t f = gen_flips(own, opp, s);
                nt_play(s, f, black);
                float v = value(discs, !black);
                nt_unplay(s, f, black);
                if (!black)
                    v = -v;
                if (sq < 0 || v > best)
                {
                    best = v;
                    sq = s;
                }
            }
        }

        bb_t f = gen_flips(own, opp, sq);
        nt_play(sq, f, black);
        own |= f | BB(sq);
        opp &= ~f;
        black_bb = black ? own : opp;
        white_bb = black ? opp : own;
        black = !black;

        if (ply >= openings && n_as < MAX_PLIES)
        {
            afterstate_t *a = &as[n_as++];
            memcpy(a->index, nt_index, sizeof a->index);
            a->discs = popcount(black_bb | white_bb);
            a->black = black;
            a->v = value(a->discs, black);
        }
        ++ply;
    }

    // The λ-returns, from the end
    float g = (float) final_score(black_bb, white_bb);
    for (int t = n_as - 1; t >= 0; --t)
    {
        afterstate_t *a = &as[t];
        if (t < n_as - 1)
            g = (float) ((1 - lambda) * as[t + 1].v + lambda * g);
        memcpy(nt_index, a->index, sizeof a->index);
        float err = g - value(a->discs, a->black);
        update(a->index, a->discs, a->black, (float) alpha * err);
    }
}

/***************************************************************
 * The match against the evaluation written by hand
 ***************************************************************/

// A move by the afterstates: the learned evaluation, or the hand one.
static int choose(bb_t own, bb_t opp, int black, int learned)
{
    int best_sq = -1;
    float best = 0;
    int discs = popcount(own | opp) + 1;
    for (bb_t m = gen_moves(own, opp); m; m &= m - 1)
    {
        int s = lowest_square(m);
        bb_t f = gen_flips(own, opp, s);
        float v;
        if (learned)
        {
            nt_play(s, f, black);
            v = value(discs, !black);
            nt_unplay(s, f, black);
            if (!black)
                v = -v;
        }
        else
            v = (float) -eval_static(opp & ~f, own | f | BB(s));
        if (best_sq < 0 || v > best)
        {
            best = v;
            best_sq = s;
        }
    }
    return best_sq;
}

// Plays the learned evaluation, black or not, against the hand one,
// from the position after the moves of opening. Returns its final disc
// difference.
static int match_game(const int *opening, int n_opening, int learned_black)
{
    bb_t black_bb = BB(28) | BB(35), white_bb = BB(27) | BB(36);
    int black = 1, ply = 0;
    nt_compute(black_bb, white_bb);
    for (;;)
    {
        bb_t own = black ? black_bb : white_bb, opp = black ? white_bb : black_bb;
        if (!gen_moves(own, opp))
        {
            if (!gen_moves(opp, own))
                break;
            black = !black;
            continue;
        }
        int sq = ply < n_opening ? opening[ply] : choose(own, opp, black, black == learned_black);
        bb_t f = gen_flips(own, opp, sq);
        nt_play(sq, f, black);
        own |= f | BB(sq);
        opp &= ~f;
        black_bb = black ? own : opp;
        white_bb = black ? opp : own;
        black = !black;
        ++ply;
    }
    int d = final_score(black_bb, white_bb);
    return learned_black ? d : -d;
}

// The score of the learned evaluation, in percent, and its mean disc
// difference, over 100 openings, each played with both colours.
static void match(double *score, double *discs)
{
    rng_t r = { 0x5EED5EED5EEDull };
    double s = 0, d = 0;
    for (int i = 0; i < 100; ++i)
    {
        // An opening of 8 random moves, in which neither side passes
        int opening[8];
        bb_t black_bb = BB(28) | BB(35), white_bb = BB(27) | BB(36);
        int black = 1;
        for (int k = 0; k < 8; ++k)
        {
            bb_t own = black ? black_bb : white_bb, opp = black ? white_bb : black_bb;
            bb_t m = gen_moves(own, opp);
            int j = (int) (rnd(&r) % (unsigned) popcount(m));
            while (j--)
                m &= m - 1;
            int sq = lowest_square(m);
            bb_t f = gen_flips(own, opp, sq);
            own |= f | BB(sq);
            opp &= ~f;
            black_bb = black ? own : opp;
            white_bb = black ? opp : own;
            black = !black;
            opening[k] = sq;
        }
        for (int c = 0; c < 2; ++c)
        {
            int diff = match_game(opening, 8, c == 0);
            s += diff > 0 ? 1 : diff == 0 ? 0.5 : 0;
            d += diff;
        }
    }
    *score = s / 2;
    *discs = d / 200;
}

/***************************************************************
 * The files
 ***************************************************************/

static int write_weights(const char *name)
{
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.tmp", name);
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return 0;
    weights_header_t h;
    memcpy(h.magic, WEIGHTS_MAGIC, sizeof h.magic);
    h.mask = NT_MASK;
    h.stages = NT_STAGES;
    h.count = NWEIGHTS;
    fwrite(&h, sizeof h, 1, f);
    for (long i = 0; i < NWEIGHTS; ++i)
    {
        double v = floor(w[i] * 100.0 + 0.5);
        if (v > 32767) v = 32767;
        if (v < -32767) v = -32767;
        int16_t x = (int16_t) v;
        fwrite(&x, sizeof x, 1, f);
    }
    if (fclose(f) != 0)
        return 0;
    return rename(tmp, name) == 0;
}

static int write_floats(const char *name)
{
    char fname[1024], tmp[1100];
    snprintf(fname, sizeof fname, "%s.float", name);
    snprintf(tmp, sizeof tmp, "%s.tmp", fname);
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return 0;
    uint32_t head[3] = { NT_MASK, NT_STAGES, NWEIGHTS };
    fwrite(head, sizeof head, 1, f);
    size_t n = fwrite(w, sizeof *w, NWEIGHTS, f);
    if (fclose(f) != 0 || n != NWEIGHTS)
        return 0;
    return rename(tmp, fname) == 0;
}

static int read_floats(const char *name)
{
    FILE *f = fopen(name, "rb");
    if (!f)
        return 0;
    uint32_t head[3];
    int ok = fread(head, sizeof head, 1, f) == 1 && head[0] == NT_MASK
          && head[1] == NT_STAGES && head[2] == NWEIGHTS
          && fread(w, sizeof *w, NWEIGHTS, f) == NWEIGHTS;
    fclose(f);
    return ok;
}

/***************************************************************
 * The threads
 ***************************************************************/
typedef struct
{
    int id;
} worker_t;

static void *worker(void *arg)
{
    worker_t *wk = arg;
    rng_t r = { seed0 * 0x9E3779B97F4A7C15ull + (uint64_t) wk->id * 0xD1B54A32D192ED03ull + 1 };
    while (__atomic_fetch_add(&games_done, 1, __ATOMIC_RELAXED) < total_games)
        play_game(&r);
    return NULL;
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void report(long games, double t0, const char *out)
{
    double score, discs;
    match(&score, &discs);
    double t = now() - t0;
    float maxw = 0;
    for (long i = 0; i < NWEIGHTS; ++i)
        if (fabsf(w[i]) > maxw)
            maxw = fabsf(w[i]);
    printf("games %ld time %.0f games/s %.0f score %.1f%% discs %+.2f max|w| %.1f\n",
           games, t, t > 0 ? games / t : 0, score, discs, maxw);
    fflush(stdout);
    if (out && (!write_weights(out) || !write_floats(out)))
        fprintf(stderr, "train: cannot write %s\n", out);
}

/***************************************************************
 * selftest
 ***************************************************************/
static int selftest(const char *out)
{
    total_games = 2000;
    rng_t r = { 42 };
    for (long g = 0; g < total_games; ++g)
        play_game(&r);
    if (!write_weights(out))
    {
        printf("selftest: cannot write %s\n", out);
        return 1;
    }

    FILE *f = fopen(out, "rb");
    weights_header_t h;
    static int16_t iw[NWEIGHTS];
    int ok = f && fread(&h, sizeof h, 1, f) == 1 && fread(iw, 2, NWEIGHTS, f) == NWEIGHTS;
    if (f)
        fclose(f);
    if (!ok)
    {
        printf("selftest: cannot read %s\n", out);
        return 1;
    }
    int n;
    int16_t *ew = engine_ntuple_weights(&n);
    memcpy(ew, iw, sizeof iw);

    int failures = 0, nonzero = 0;
    for (int i = 0; i < 1000; ++i)
    {
        // A random position, by random moves
        bb_t black_bb = BB(28) | BB(35), white_bb = BB(27) | BB(36);
        int black = 1;
        int plies = 1 + (int) (rnd(&r) % 50);
        for (int k = 0; k < plies; ++k)
        {
            bb_t own = black ? black_bb : white_bb, opp = black ? white_bb : black_bb;
            bb_t m = gen_moves(own, opp);
            if (!m)
                break;
            int j = (int) (rnd(&r) % (unsigned) popcount(m));
            while (j--)
                m &= m - 1;
            int sq = lowest_square(m);
            bb_t fl = gen_flips(own, opp, sq);
            own |= fl | BB(sq);
            opp &= ~fl;
            black_bb = black ? own : opp;
            white_bb = black ? opp : own;
            black = !black;
        }
        int discs = popcount(black_bb | white_bb);
        nt_compute(black_bb, white_bb);
        float v = value(discs, black);
        // The engine's value, for the side to move, in hundredths
        int e = nt_eval(black, discs);
        float vb = black ? v : -v;
        nt_compute(white_bb, black_bb);    // The colours swapped
        float vs = value(discs, !black);
        nonzero += fabsf(v) > 0.01f;
        // The antisymmetry, and the rounding of each of the places
        if (fabsf(v + vs) > 0.001f * (1 + fabsf(v))
            || (fabsf(vb * 100) < EVAL_MAX - 100 && fabsf(vb * 100 - e) > 0.5f * (nt_places + 1) + 1))
        {
            printf("selftest: position %d: value %.3f, swapped %.3f, engine %d\n", i, v, vs, e);
            ++failures;
        }
    }
    if (nonzero < 500)
    {
        printf("selftest: only %d of 1000 values are not 0: nothing was learned\n", nonzero);
        ++failures;
    }
    printf("selftest %s\n", failures ? "FAIL" : "ok");
    return failures != 0;
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: train [OPTIONS] OUTPUT\n"
            "  --games N      games to play (%ld)\n"
            "  --threads N    threads (%d)\n"
            "  --alpha A      the rate of learning (%g)\n"
            "  --lambda L     the λ of TD(λ) (%g)\n"
            "  --epsilon E    the share of moves at random (%g)\n"
            "  --opening N    up to N random moves at the start of a game (%d)\n"
            "  --report N     the match and OUTPUT every N games (%ld)\n"
            "  --seed S       the seed of the random moves (%llu)\n"
            "  --init FILE    start from the weights of FILE, an OUTPUT.float\n"
            "  --selftest     the selftest, which writes OUTPUT\n",
            total_games, threads, alpha, lambda, epsilon, opening_moves, report_every,
            (unsigned long long) seed0);
    exit(2);
}

int main(int argc, char **argv)
{
    const char *init = NULL, *out = NULL;
    int self = 0;

    for (int i = 1; i < argc; ++i)
    {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--selftest")) { self = 1; continue; }
        if (a[0] != '-')
        {
            out = a;
            continue;
        }
        if (!v)
            usage();
        ++i;
        if (!strcmp(a, "--games")) total_games = atol(v);
        else if (!strcmp(a, "--threads")) threads = atoi(v);
        else if (!strcmp(a, "--alpha")) alpha = atof(v);
        else if (!strcmp(a, "--lambda")) lambda = atof(v);
        else if (!strcmp(a, "--epsilon")) epsilon = atof(v);
        else if (!strcmp(a, "--opening")) opening_moves = atoi(v);
        else if (!strcmp(a, "--report")) report_every = atol(v);
        else if (!strcmp(a, "--seed")) seed0 = strtoull(v, NULL, 10);
        else if (!strcmp(a, "--init")) init = v;
        else usage();
    }
    if (!out || threads < 1 || threads > 256 || report_every < 1)
        usage();

    nt_init();
    init_swapped();
    w = calloc(NWEIGHTS, sizeof *w);
    if (!w)
    {
        fprintf(stderr, "train: out of memory\n");
        return 1;
    }
    if (init && !read_floats(init))
    {
        fprintf(stderr, "train: %s is not weights of these patterns and stages\n", init);
        return 1;
    }
    if (self)
        return selftest(out);

    printf("patterns 0x%X stages %d places %d weights %d threads %d alpha %g lambda %g epsilon %g\n",
           NT_MASK, NT_STAGES, nt_places, NWEIGHTS, threads, alpha, lambda, epsilon);
    double t0 = now();
    report(0, t0, NULL);

    pthread_t tid[256];
    worker_t wk[256];
    for (int i = 0; i < threads; ++i)
    {
        wk[i].id = i;
        pthread_create(&tid[i], NULL, worker, &wk[i]);
    }

    // The reports, as the games go on
    long next = report_every;
    while (next <= total_games)
    {
        struct timespec ts = { 0, 50000000 };
        long g = __atomic_load_n(&games_done, __ATOMIC_RELAXED);
        if (g < next)
        {
            nanosleep(&ts, NULL);
            continue;
        }
        report(next, t0, out);
        next += report_every;
    }
    for (int i = 0; i < threads; ++i)
        pthread_join(tid[i], NULL);
    if (total_games % report_every)
        report(total_games, t0, out);
    return 0;
}
