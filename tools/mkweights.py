#!/usr/bin/env python3
"""mkweights.py - writes the weights of a file of tools/train.c as C.

Usage: mkweights.py WEIGHTS MASK STAGES OUTPUT

Reads WEIGHTS, in the format of src/ui/weights_file.h, checks that it
is of the patterns MASK in STAGES stages, and writes OUTPUT: the
definition of ntuple.c's table of weights with them, which ntuple.c
includes when NT_WEIGHTS names the file. The C checks NT_MASK and
NT_STAGES too, so a build with other patterns fails to compile.
"""

from __future__ import annotations

import os
import struct
import sys


def main() -> int:
    if len(sys.argv) != 5:
        print(__doc__, file=sys.stderr)
        return 2
    path, mask, stages, out = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3]), sys.argv[4]
    data = open(path, 'rb').read()
    if len(data) < 20 or data[:8] != b'NOTHW001':
        print(f'mkweights.py: {path} is not a file of weights', file=sys.stderr)
        return 1
    fmask, fstages, count = struct.unpack('<III', data[8:20])
    if (fmask, fstages) != (mask, stages):
        print(f'mkweights.py: {path} has the patterns 0x{fmask:X} in {fstages} stages, '
              f'not 0x{mask:X} in {stages}', file=sys.stderr)
        return 1
    if len(data) != 20 + 2 * count:
        print(f'mkweights.py: {path} has {(len(data) - 20) // 2} weights, not {count}', file=sys.stderr)
        return 1
    weights = struct.unpack(f'<{count}h', data[20:])

    with open(out + '.tmp', 'w') as f:
        f.write(f'// Made by tools/mkweights.py from {path}. Do not edit.\n')
        f.write(f'#if NT_MASK != 0x{mask:X} || NT_STAGES != {stages}\n')
        f.write(f'#error "the weights are of the patterns 0x{mask:X} in {stages} stages"\n')
        f.write('#endif\n')
        f.write(f'static int16_t weights[{count}] =\n{{\n')
        for i in range(0, count, 16):
            f.write('    ' + ','.join(str(w) for w in weights[i:i + 16]) + ',\n')
        f.write('};\n')
    os.replace(out + '.tmp', out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
