/***************************************************************
 * search.c: the search of the middle game, and iterative deepening.
 *
 * A negamax alpha-beta search, fail-soft, as a principal variation
 * search: the first move of a node with the full window, and the
 * others with a null window, searched again with the full window if
 * one turns out better. A pass does not count as a ply of the depth.
 * A node whose depth reaches the end of the game is solved by
 * solve.c instead.
 *
 * The moves are tried in this order: the move of the transposition
 * table, then, with 2 plies or more to go, the move that leaves the
 * opponent the fewest moves, and then by the square (square_value).
 *
 * With the N-tuple evaluation, the indexes of ntuple.c are kept up to
 * date around each move; the solver needs none.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

#define INF 10000

// The iterations before an exact solve, which order the moves.
#ifndef SOLVE_PREP_DEPTH
#define SOLVE_PREP_DEPTH 4
#endif

#define ORDER_MOBILITY 16
#define ORDER_TT 10000

// The value of a square for the order of the moves: the corners
// first, the squares next to them last.
const int8_t square_value[64] =
{
    20, -3, 11,  8,  8, 11, -3, 20,
    -3, -7, -4,  1,  1, -4, -7, -3,
    11, -4,  2,  2,  2,  2, -4, 11,
     8,  1,  2, -3, -3,  2,  1,  8,
     8,  1,  2, -3, -3,  2,  1,  8,
    11, -4,  2,  2,  2,  2, -4, 11,
    -3, -7, -4,  1,  1, -4, -7, -3,
    20, -3, 11,  8,  8, 11, -3, 20,
};

bb_t    ms_flips[MOVE_STACK_SIZE];
int16_t ms_score[MOVE_STACK_SIZE];
uint8_t ms_square[MOVE_STACK_SIZE];
int     ms_top;

int search_stopped;
// The N-tuple evaluation, if the build has its weights (NT_WEIGHTS).
#ifdef NT_WEIGHTS
int eval_kind = ENGINE_EVAL_NTUPLE;
#else
int eval_kind = ENGINE_EVAL_HAND;
#endif
static uint32_t nodes;
static engine_limits_t limits;

int count_node(void)
{
    ++nodes;
    if (limits.nodes && nodes >= limits.nodes)
        search_stopped = 1;
    else if ((nodes & 1023) == 0 && limits.stop && limits.stop())
        search_stopped = 1;
    return search_stopped;
} // end of count_node

void pick_best(int base, int i, int n)
{
    int b = base + i;
    for (int j = base + i + 1; j < base + n; ++j)
        if (ms_score[j] > ms_score[b])
            b = j;
    if (b != base + i)
    {
        int a = base + i;
        bb_t f = ms_flips[a]; ms_flips[a] = ms_flips[b]; ms_flips[b] = f;
        int16_t s = ms_score[a]; ms_score[a] = ms_score[b]; ms_score[b] = s;
        uint8_t q = ms_square[a]; ms_square[a] = ms_square[b]; ms_square[b] = q;
    }
} // end of pick_best

// a / b, rounded down and up, for b > 0.
static int div_floor(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static int div_ceil(int a, int b)  { return a >= 0 ? (a + b - 1) / b : -(-a / b); }

int evaluate(bb_t own, bb_t opp, int black)
{
    if (eval_kind == ENGINE_EVAL_NTUPLE)
        return nt_eval(black, popcount(own | opp));
    return eval_static(own, opp);
} // end of evaluate

/***************************************************************
 * search
 * The value of the position for own, depth plies deep, between
 * alpha and beta, fail-soft. black is whether own is black.
 ***************************************************************/
static int search(bb_t own, bb_t opp, int depth, int alpha, int beta, int black)
{
    int empties = 64 - popcount(own | opp);
    if (depth >= empties)
        return 100 * solve(own, opp, div_floor(alpha, 100), div_ceil(beta, 100), empties);

    if (count_node())
        return 0;
#ifdef CHECK_STATE
    assert(eval_kind != ENGINE_EVAL_NTUPLE || nt_consistent(black ? own : opp, black ? opp : own));
#endif
    if (depth == 0)
        return evaluate(own, opp, black);

    bb_t moves = gen_moves(own, opp);
    if (!moves)
    {
        if (!gen_moves(opp, own))
            return 100 * final_score(own, opp);
        return -search(opp, own, depth, -beta, -alpha, !black);
    }

    int tt_move = TT_NO_MOVE;
    tt_hit_t hit;
    if (tt_probe(own, opp, &hit))
    {
        tt_move = hit.move;
        if (hit.depth >= depth)
        {
            if (hit.bound == TT_EXACT)
                return hit.score;
            if (hit.bound == TT_LOWER && hit.score > alpha)
                alpha = hit.score;
            if (hit.bound == TT_UPPER && hit.score < beta)
                beta = hit.score;
            if (alpha >= beta)
                return hit.score;
        }
    }

    // The moves, in their order
    int base = ms_top;
    int n = 0;
    assert(base + popcount(moves) <= MOVE_STACK_SIZE);
    while (moves)
    {
        int sq = lowest_square(moves);
        moves &= moves - 1;
        bb_t f = gen_flips(own, opp, sq);
        int score = square_value[sq];
        if (sq == tt_move)
            score = ORDER_TT;
        else if (depth >= 2)
            score -= ORDER_MOBILITY * popcount(gen_moves(opp & ~f, own | f | BB(sq)));
        ms_square[base + n] = (uint8_t) sq;
        ms_flips[base + n] = f;
        ms_score[base + n] = (int16_t) score;
        ++n;
    }
    ms_top = base + n;

    int a0 = alpha;
    int best = -INF, best_move = TT_NO_MOVE;
    for (int i = 0; i < n; ++i)
    {
        pick_best(base, i, n);
        int sq = ms_square[base + i];
        bb_t f = ms_flips[base + i];
        bb_t child_own = opp & ~f, child_opp = own | f | BB(sq);
        int nt = eval_kind == ENGINE_EVAL_NTUPLE;
        if (nt)
            nt_play(sq, f, black);
        int v;
        if (i == 0)
            v = -search(child_own, child_opp, depth - 1, -beta, -alpha, !black);
        else
        {
            v = -search(child_own, child_opp, depth - 1, -alpha - 1, -alpha, !black);
            if (v > alpha && v < beta && !search_stopped)
                v = -search(child_own, child_opp, depth - 1, -beta, -alpha, !black);
        }
        if (nt)
            nt_unplay(sq, f, black);
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

    tt_store(own, opp, depth, best <= a0 ? TT_UPPER : best >= beta ? TT_LOWER : TT_EXACT,
             best, best_move);
    return best;
} // end of search

/***************************************************************
 * The root
 ***************************************************************/
static int root_moves[ENGINE_MAX_MOVES];
static int root_n;
static int root_done;   // The moves of the iteration searched so far

/***************************************************************
 * root_search
 * The best move of the root, depth plies deep, into *best_move, and
 * its score. The best move goes to the front of root_moves, ahead of
 * the others, which keep their order.
 ***************************************************************/
static int root_search(int depth, int alpha, int beta, int *best_move)
{
    bb_t own = board.own, opp = board.opp;
    int black = board.side == ENGINE_BLACK;
    int best = -INF;

    root_done = 0;
    for (int i = 0; i < root_n; ++i)
    {
        int sq = root_moves[i];
        bb_t f = gen_flips(own, opp, sq);
        bb_t child_own = opp & ~f, child_opp = own | f | BB(sq);
        int nt = eval_kind == ENGINE_EVAL_NTUPLE;
        if (nt)
            nt_play(sq, f, black);
        int v;
        if (i == 0)
            v = -search(child_own, child_opp, depth - 1, -beta, -alpha, !black);
        else
        {
            v = -search(child_own, child_opp, depth - 1, -alpha - 1, -alpha, !black);
            if (v > alpha && v < beta && !search_stopped)
                v = -search(child_own, child_opp, depth - 1, -beta, -alpha, !black);
        }
        if (nt)
            nt_unplay(sq, f, black);
        if (search_stopped)
            break;
        ++root_done;
        if (v > best)
        {
            best = v;
            *best_move = sq;
            for (int j = i; j > 0; --j)
                root_moves[j] = root_moves[j - 1];
            root_moves[0] = sq;
            if (v > alpha)
            {
                alpha = v;
                if (v >= beta)
                    break;
            }
        }
    }
    return best;
} // end of root_search

/***************************************************************
 * get_pv
 * The principal variation: the best move, and then the moves of the
 * table, as long as they are legal.
 ***************************************************************/
static void get_pv(int move, engine_info_t *info)
{
    bb_t own = board.own, opp = board.opp;
    info->pv_len = 0;

    while (info->pv_len < ENGINE_MAX_PV)
    {
        if (move == ENGINE_PASS)
        {
            if (gen_moves(own, opp) || !gen_moves(opp, own))
                break;
        }
        else
        {
            if (move < 0 || move >= 64 || ((own | opp) & BB(move)))
                break;
            bb_t f = gen_flips(own, opp, move);
            if (!f)
                break;
            own |= f | BB(move);
            opp &= ~f;
        }
        info->pv[info->pv_len++] = move;
        bb_t t = own; own = opp; opp = t;

        tt_hit_t hit;
        if (!tt_probe(own, opp, &hit) || hit.move == TT_NO_MOVE)
        {
            // A pass is not in the table, as search makes it in place
            if (!gen_moves(own, opp) && gen_moves(opp, own))
                move = ENGINE_PASS;
            else
                break;
        }
        else
            move = hit.move;
    }
} // end of get_pv

int engine_search(const engine_limits_t *lim, engine_info_fn info_fn)
{
    int list[ENGINE_MAX_MOVES];
    int n = engine_legal_moves(list);
    if (n == 0)
        return ENGINE_NO_MOVE;
    if (list[0] == ENGINE_PASS)
        return ENGINE_PASS;

    limits = *lim;
    nodes = 0;
    search_stopped = 0;
    ms_top = 0;

    // The first order: as search orders the moves of a node
    bb_t own = board.own, opp = board.opp;
    root_n = n;
    for (int i = 0; i < n; ++i)
    {
        int sq = list[i];
        bb_t f = gen_flips(own, opp, sq);
        ms_square[i] = (uint8_t) sq;
        ms_flips[i] = f;
        ms_score[i] = (int16_t) (square_value[sq]
            - ORDER_MOBILITY * popcount(gen_moves(opp & ~f, own | f | BB(sq))));
    }
    for (int i = 0; i < n; ++i)
    {
        pick_best(0, i, n);
        root_moves[i] = ms_square[i];
    }

    int empties = 64 - popcount(own | opp);
    int best_move = root_moves[0];
    int depth = 1;
    for (;;)
    {
        int exact = depth >= empties;
        int wld = exact && limits.wld;
        int move = best_move;
        int score = root_search(depth, wld ? -100 : -INF, wld ? 100 : INF, &move);
        if (search_stopped && root_done == 0)
            break;
        best_move = move;

        if (info_fn)
        {
            engine_info_t info;
            info.depth = exact ? empties : depth;
            info.exact = !exact ? ENGINE_INEXACT : wld ? ENGINE_WLD : ENGINE_EXACT;
            info.score = !wld ? score : score > 0 ? 100 : score < 0 ? -100 : 0;
            info.nodes = nodes;
            get_pv(best_move, &info);
            // The table may hold moves of deeper searches
            if (!exact && info.pv_len > depth)
                info.pv_len = depth;
            if (info_fn(&info))
                break;
        }
        if (search_stopped || exact || (limits.depth && depth >= limits.depth))
            break;
        if (limits.solve && empties <= limits.solve && depth >= SOLVE_PREP_DEPTH
            && (!limits.depth || empties <= limits.depth))
            depth = empties;
        else
            ++depth;
    }
    return best_move;
} // end of engine_search

int engine_eval(void)
{
    return evaluate(board.own, board.opp, board.side == ENGINE_BLACK);
} // end of engine_eval

void engine_set_eval(int kind)
{
    eval_kind = kind;
    tt_clear();
} // end of engine_set_eval
