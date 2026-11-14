/***************************************************************
 * ntuple.c: the N-tuple evaluation: a sum of weights looked up in
 * tables, one table for each pattern of squares.
 *
 * The patterns are the candidates of the README. Each is given by the
 * squares of one place, in an order; its other places are the images
 * of that place under the 8 symmetries of the board, each kept unless
 * it is the same squares in the same order as one kept before. So a
 * place that a symmetry maps onto itself, such as an edge, is read in
 * both directions, and the evaluation of a position is the same as that
 * of its images: all places of a pattern share one table.
 *
 * The state of a place is its index: the sum over its squares of
 * digit * 3^k, where k is the square's place in the order, and the
 * digit is 0 for an empty square, 1 for a black disc, and 2 for a white
 * one. The indexes are of the whole board, from black's view, so that a
 * move changes only those of the squares it fills and flips. They are
 * kept up to date by nt_play and nt_unplay.
 *
 * The weights: NT_STAGES stages, by the number of discs, each of
 * nt_stage_size() weights in hundredths of a disc: the tables of the
 * patterns of NT_MASK, in the order of nt_defs, and last the weight of
 * having the move. The value of a position for black is the sum of the
 * weights of its places, and for the side to move that, or its
 * negation for white, plus the weight of having the move.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

// The squares, by name
enum
{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
};

const nt_def_t nt_defs[NT_PATTERNS] =
{
    { "corner3x3", 9,  { A1, B1, C1, A2, B2, C2, A3, B3, C3 } },
    { "corner2x5", 10, { A1, B1, C1, D1, E1, A2, B2, C2, D2, E2 } },
    { "edge2x",    10, { B2, A1, B1, C1, D1, E1, F1, G1, H1, G2 } },
    { "row1",      8,  { A1, B1, C1, D1, E1, F1, G1, H1 } },
    { "row2",      8,  { A2, B2, C2, D2, E2, F2, G2, H2 } },
    { "row3",      8,  { A3, B3, C3, D3, E3, F3, G3, H3 } },
    { "row4",      8,  { A4, B4, C4, D4, E4, F4, G4, H4 } },
    { "diag8",     8,  { A1, B2, C3, D4, E5, F6, G7, H8 } },
    { "diag7",     7,  { B1, C2, D3, E4, F5, G6, H7 } },
    { "diag6",     6,  { C1, D2, E3, F4, G5, H6 } },
    { "diag5",     5,  { D1, E2, F3, G4, H5 } },
    { "diag4",     4,  { E1, F2, G3, H4 } },
};

// The places: their pattern, squares, and the offset of their table
// in the weights of a stage.
int nt_places;
uint8_t nt_place_pattern[NT_MAX_PLACES];
uint8_t nt_place_squares[NT_MAX_PLACES][NT_MAX_SQUARES];
static uint32_t place_offset[NT_MAX_PLACES];

// For each square, the places it is in, and its power of 3 in each:
// the entries from square_first[sq] to square_first[sq + 1] - 1.
static uint16_t square_first[65];
static uint8_t  entry_place[NT_MAX_ENTRIES];
static uint16_t entry_power[NT_MAX_ENTRIES];

uint16_t nt_index[NT_MAX_PLACES];

static int16_t weights[NT_STAGES * NT_STAGE_SIZE];

static int initialized;

// The image of square sq under symmetry s, 0 to 7.
static int transform(int sq, int s)
{
    int r = sq / 8, c = sq % 8, t;
    if (s & 4)
    {
        t = r; r = c; c = t;    // Transpose
    }
    if (s & 2)
        r = 7 - r;
    if (s & 1)
        c = 7 - c;
    return r * 8 + c;
} // end of transform

static uint32_t power3(int n)
{
    uint32_t p = 1;
    while (n--)
        p *= 3;
    return p;
} // end of power3

void nt_init(void)
{
    if (initialized)
        return;
    initialized = 1;

    uint32_t offset = 0;
    nt_places = 0;
    for (int p = 0; p < NT_PATTERNS; ++p)
    {
        if (!((NT_MASK >> p) & 1))
            continue;
        const nt_def_t *d = &nt_defs[p];
        for (int s = 0; s < 8; ++s)
        {
            uint8_t sq[NT_MAX_SQUARES];
            for (int k = 0; k < d->n; ++k)
                sq[k] = (uint8_t) transform(d->squares[k], s);
            int seen = 0;
            for (int q = nt_places - 1; q >= 0 && nt_place_pattern[q] == p && !seen; --q)
            {
                int same = 1;
                for (int k = 0; k < d->n; ++k)
                    same &= nt_place_squares[q][k] == sq[k];
                seen = same;
            }
            if (seen)
                continue;
            assert(nt_places < NT_MAX_PLACES);
            nt_place_pattern[nt_places] = (uint8_t) p;
            for (int k = 0; k < d->n; ++k)
                nt_place_squares[nt_places][k] = sq[k];
            place_offset[nt_places] = offset;
            ++nt_places;
        }
        offset += power3(d->n);
    }
    assert(offset + 1 == NT_STAGE_SIZE);

    // The entries of each square
    int e = 0;
    for (int sq = 0; sq < 64; ++sq)
    {
        square_first[sq] = (uint16_t) e;
        for (int q = 0; q < nt_places; ++q)
        {
            const nt_def_t *d = &nt_defs[nt_place_pattern[q]];
            for (int k = 0; k < d->n; ++k)
                if (nt_place_squares[q][k] == sq)
                {
                    assert(e < NT_MAX_ENTRIES);
                    entry_place[e] = (uint8_t) q;
                    entry_power[e] = (uint16_t) power3(k);
                    ++e;
                }
        }
    }
    square_first[64] = (uint16_t) e;
} // end of nt_init

int nt_place_index(int q, bb_t black, bb_t white)
{
    const nt_def_t *d = &nt_defs[nt_place_pattern[q]];
    int index = 0;
    for (int k = d->n - 1; k >= 0; --k)
    {
        int sq = nt_place_squares[q][k];
        index = index * 3 + ((black & BB(sq)) ? 1 : (white & BB(sq)) ? 2 : 0);
    }
    return index;
} // end of nt_place_index

void nt_compute(bb_t black, bb_t white)
{
    for (int q = 0; q < nt_places; ++q)
        nt_index[q] = (uint16_t) nt_place_index(q, black, white);
} // end of nt_compute

int nt_consistent(bb_t black, bb_t white)
{
    for (int q = 0; q < nt_places; ++q)
        if (nt_index[q] != nt_place_index(q, black, white))
            return 0;
    return 1;
} // end of nt_consistent

/***************************************************************
 * nt_play, nt_unplay
 * A disc of black (or white, if black is 0) on sq, which flips the
 * discs of flips: the digit of sq goes from 0 to 1 (or 2), and those
 * of flips from 2 to 1 (or 1 to 2). nt_unplay takes it back.
 ***************************************************************/
void nt_play(int sq, bb_t flips, int black)
{
    int last = square_first[sq + 1];
    for (int e = square_first[sq]; e < last; ++e)
        nt_index[entry_place[e]] += black ? entry_power[e] : 2 * entry_power[e];
    while (flips)
    {
        int s = lowest_square(flips);
        flips &= flips - 1;
        last = square_first[s + 1];
        if (black)
            for (int e = square_first[s]; e < last; ++e)
                nt_index[entry_place[e]] -= entry_power[e];
        else
            for (int e = square_first[s]; e < last; ++e)
                nt_index[entry_place[e]] += entry_power[e];
    }
} // end of nt_play

void nt_unplay(int sq, bb_t flips, int black)
{
    int last = square_first[sq + 1];
    for (int e = square_first[sq]; e < last; ++e)
        nt_index[entry_place[e]] -= black ? entry_power[e] : 2 * entry_power[e];
    while (flips)
    {
        int s = lowest_square(flips);
        flips &= flips - 1;
        last = square_first[s + 1];
        if (black)
            for (int e = square_first[s]; e < last; ++e)
                nt_index[entry_place[e]] += entry_power[e];
        else
            for (int e = square_first[s]; e < last; ++e)
                nt_index[entry_place[e]] -= entry_power[e];
    }
} // end of nt_unplay

int nt_stage(int discs)
{
    return (discs - 4) * NT_STAGES / 61;
} // end of nt_stage

int nt_eval(int black, int discs)
{
    const int16_t *w = weights + nt_stage(discs) * NT_STAGE_SIZE;
    int32_t sum = 0;
    for (int q = 0; q < nt_places; ++q)
        sum += w[place_offset[q] + nt_index[q]];
    if (!black)
        sum = -sum;
    sum += w[NT_STAGE_SIZE - 1];
    if (sum > EVAL_MAX)
        sum = EVAL_MAX;
    if (sum < -EVAL_MAX)
        sum = -EVAL_MAX;
    return (int) sum;
} // end of nt_eval

int16_t *engine_ntuple_weights(int *n)
{
    *n = NT_STAGES * NT_STAGE_SIZE;
    return weights;
} // end of engine_ntuple_weights
