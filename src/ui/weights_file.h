/***************************************************************
 * weights_file.h: the format of a file of weights of the N-tuple
 * evaluation, which tools/train.c writes and the text front end loads
 * (weights FILE): this header, little-endian, and then count weights,
 * 16-bit integers in hundredths of a disc, as engine_ntuple_weights
 * lays them out. mask and stages must be those of the build
 * (engine_ntuple_layout).
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#ifndef WEIGHTS_FILE_H
#define WEIGHTS_FILE_H

#include <stdint.h>

#define WEIGHTS_MAGIC "NOTHW001"

typedef struct
{
    char     magic[8];   // WEIGHTS_MAGIC, without its ending 0
    uint32_t mask;       // NT_MASK
    uint32_t stages;     // NT_STAGES
    uint32_t count;      // The weights that follow
} weights_header_t;

#endif // WEIGHTS_FILE_H
