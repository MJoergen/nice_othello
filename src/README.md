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
| `eval.c` | The evaluation written by hand: mobility, potential mobility, corners, X- and C-squares. The baseline for the learned one. |
| `search.c` | Iterative deepening, and the search of the middle game: PVS with the transposition table, moves ordered by the opponent's mobility. The principal variation. |
| `solve.c` | The exact solver of the end of the game: fastest first, then parity near the end. |
| `tt.c` | The transposition table, keyed by a hash of the two bitboards. |
| `ntuple.c` | The N-tuple evaluation: the patterns, their places under the symmetries of the board, the indexes kept up to date by `nt_play` and `nt_unplay`, and the sum of the weights. |

The position is kept from the side to move: `own` its discs, `opp` the
opponent's. A move swaps them. The squares are a1 = 0 to h8 = 63, row
by row, row 1 at the top.

Scores are in hundredths of a disc, for the side to move; the end of
the game is 100 times the disc difference, with the empty squares to
the winner. The solver works in whole discs. A node of the search
whose depth reaches the end of the game is handed to the solver.

The evaluation is the one written by hand (`eval.c`) or the N-tuple
network (`ntuple.c`), chosen by `engine_set_eval`. The indexes of the
N-tuple network are of the whole board, from black's view: `board.c`
keeps those of the game, and the search updates them around each move
when the N-tuple evaluation is in use. The patterns and the stages are
chosen at build time (`NT_MASK`, `NT_STAGES`).

## src/platform/

| File | What it does |
|---|---|
| `platform.h` | What a front end needs from the machine: the clock, the console's input, the files (none on NICE), and defaults such as the time of a move and the empty squares at which to solve. |
| `nice.c` | For NICE: niceemu, or the MEGA65 under the monitor. |
| `host.c` | For a PC. |

## src/ui/

| File | What it does |
|---|---|
| `text.c` | The text front end on the console: set up positions, make and take back moves, search (`go`), solve, choose the evaluation, load weights, perft. |
| `version.h` | The version of the build, which the Makefile writes into `build/gen/version.c`. |
| `weights_file.h` | The format of a file of N-tuple weights, which `tools/train.c` writes and `weights FILE` loads. |
