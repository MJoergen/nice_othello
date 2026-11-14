/***************************************************************
 * tt.c: the transposition table.
 *
 * The key of a position is a hash of its two bitboards: each is
 * multiplied by a constant, which the translator writes inline for
 * NICE, and the bits are mixed as in splitmix64. Its low TT_BITS bits
 * are the index, and its high 32 bits are kept in the entry, to tell
 * the positions of an index apart. An entry is two words:
 *
 *   check   the high 32 bits of the key
 *   data    bits  0-15: the score
 *           bits 16-23: the move, a square, ENGINE_PASS, or TT_NO_MOVE
 *           bits 24-29: the depth, or TT_SOLVED
 *           bits 30-31: the kind of bound
 *
 * The depth is at least 1, so an entry with data 0 is empty.
 * A new entry always replaces the old one.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include "internal.h"

typedef struct
{
    uint32_t check;
    uint32_t data;
} tt_entry_t;

static tt_entry_t tt[1 << TT_BITS];

#define TT_MASK ((1u << TT_BITS) - 1)

static uint64_t tt_key(bb_t own, bb_t opp)
{
    uint64_t k = own * 0x9E3779B97F4A7C15ull ^ opp * 0xC2B2AE3D27D4EB4Full;
    k ^= k >> 31;
    k *= 0xBF58476D1CE4E5B9ull;
    k ^= k >> 29;
    return k;
} // end of tt_key

void tt_clear(void)
{
    for (uint32_t i = 0; i <= TT_MASK; ++i)
    {
        tt[i].check = 0;
        tt[i].data = 0;
    }
} // end of tt_clear

int tt_probe(bb_t own, bb_t opp, tt_hit_t *hit)
{
    uint64_t k = tt_key(own, opp);
    const tt_entry_t *e = &tt[(uint32_t) k & TT_MASK];
    if (e->check != (uint32_t) (k >> 32) || !e->data)
        return 0;
    hit->score = (int16_t) (e->data & 0xFFFF);
    hit->move = (int) ((e->data >> 16) & 0xFF);
    hit->depth = (int) ((e->data >> 24) & 0x3F);
    hit->bound = (int) (e->data >> 30);
    return 1;
} // end of tt_probe

void tt_store(bb_t own, bb_t opp, int depth, int bound, int score, int move)
{
    assert(depth >= 1 && depth <= TT_SOLVED);
    assert(score >= -32768 && score <= 32767);
    assert(move == TT_NO_MOVE || (move >= 0 && move <= ENGINE_PASS));
    uint64_t k = tt_key(own, opp);
    tt_entry_t *e = &tt[(uint32_t) k & TT_MASK];
    // The move of the position's old entry, kept when the new one has none
    if (move == TT_NO_MOVE && e->data && e->check == (uint32_t) (k >> 32))
        move = (int) ((e->data >> 16) & 0xFF);
    e->check = (uint32_t) (k >> 32);
    e->data = ((uint32_t) score & 0xFFFF) | ((uint32_t) move << 16)
            | ((uint32_t) depth << 24) | ((uint32_t) bound << 30);
} // end of tt_store
