/***************************************************************
 * levels.h: the levels of the front ends, 1 to 5. A level sets the
 * depth of the search, and the number of empty squares at which it
 * solves the game exactly. LOG.md has the time of a move on the
 * MEGA65 at each, and how each does against the one below.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/
#ifndef LEVELS_H
#define LEVELS_H

#include <stdint.h>
#include "../engine/engine.h"

#define LEVELS 5

static const uint8_t level_depth[LEVELS] = {1, 2, 4, 6, 7};
static const uint8_t level_solve[LEVELS] = {8, 10, 12, 14, 16};

// Sets the depth and solve of limits for the level, 1 to LEVELS, in
// the position of the engine.
static inline void level_limits(int level, engine_limits_t *limits)
{
    int black, white;
    engine_count(&black, &white);
    int empties = 64 - black - white;
    limits->solve = level_solve[level - 1];
    limits->depth = empties <= limits->solve ? empties : level_depth[level - 1];
}

#endif // LEVELS_H
