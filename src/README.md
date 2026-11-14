# The source

## src/engine/: the core

No I/O, no clock, and no malloc, so that the same files build for NICE
and for the host.

| File | What it does |
|---|---|
| `engine.h` | The interface, for the front ends and the tests. |
| `internal.h` | What the engine's files share: the bitboards, the position, the history. |
| `board.c` | The position and the game: setting up, the text of a position, the legal moves, making a move, undo, and the names of moves. |
| `movegen.c` | The legal moves and the flips of a move, for all directions at once, by shifts and masks of the bitboards; popcount. |
| `perft.c` | Perft, the count of the leaves of the tree of moves. |
| `check.c` | A plain move generator, square by square, to test `movegen.c` against. |

The position is kept from the side to move: `own` its discs, `opp` the
opponent's. A move swaps them. The squares are a1 = 0 to h8 = 63, row
by row, row 1 at the top.

## src/platform/

| File | What it does |
|---|---|
| `platform.h` | What a front end needs from the machine: the clock, and the console's input. |
| `nice.c` | For NICE: niceemu, or the MEGA65 under the monitor. |
| `host.c` | For a PC. |

## src/ui/

| File | What it does |
|---|---|
| `text.c` | The text front end on the console: set up positions, make and take back moves, perft. |
| `version.h` | The version of the build, which the Makefile writes into `build/gen/version.c`. |
