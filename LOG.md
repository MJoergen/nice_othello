# Log

Investigations made during the development (doc/HISTORY.md), newest first.

## 2026-11-14: The cost of the N-tuple evaluation on NICE, at Stage 3

The text front end in niceemu, `go depth 6` from the start, with the
patterns of the tests (`NT_MASK` 0xF98: row1, row2, and the 5
diagonals, 52 places), in steps a call (nice_chess's `tools/prof.c`):

| Function | Steps |
|---|---:|
| `nt_eval` | 644 |
| `nt_play` | 182 |
| `nt_unplay` | 182 |
| `eval_static`, the hand-written one | 257 |
| `gen_moves`, which it calls twice | 435 |
| `nt_init`, once | 288999 |

So a leaf of the N-tuple evaluation costs about 644 steps, against
about 1130 for the hand-written one, and each move of the search 364
more for the indexes. `nt_eval` is about 12 steps a place; with all the
patterns, 92 places, it would be about 1150. The weights were 0, so the
two searches visited different trees (959 and 541 nodes), and their
totals do not compare.

The MEGA65 build needs a third page of linear memory for the weights,
45848 of them, 92 KB: the build needs 66038 words of the 94176, with
the table's 16384 and the weights' 22924.

## 2026-11-14: The speed of the search and the solver, at Stage 2

In niceemu, the same searches as on the host, node for node
(`TT_BITS` 13):

- The middle game: about 2350 steps a node. `go depth 6` from the
  start, 959 nodes, takes 2.26 million steps, 0.04 s at 54 million
  steps a second. The evaluation calls `gen_moves` twice, and the order
  of the moves once for each move of a node with 2 plies or more to go.
- The solver: about 1200 steps a node. FFO 1 to 3, of 14 empty squares,
  take 81426, 36558, and 144842 nodes, 315 million steps for the three:
  1.5, 0.7, and 2.7 s on the MEGA65 at 54 million steps a second. So
  NICE solves at 14 empty squares by default (`platform_solve_empties`),
  within the 5 s of a move.

On the host (`TT_BITS` 20, gcc -O2), the solver does about 17 million
nodes a second. FFO 1 to 19 (14 to 16 empty squares) take 0.2 s in
all. FFO 20 to 38:

| FFO | Empty | Nodes | Seconds |
|---:|---:|---:|---:|
| 21 | 15 | 553522 | 0.04 |
| 24 | 19 | 7202727 | 0.43 |
| 26 | 20 | 23656714 | 1.41 |
| 33 | 20 | 29951971 | 1.76 |
| 36 | 21 | 58967431 | 3.51 |
| 37 | 22 | 80650804 | 4.88 |
| 38 | 24 | 569667882 | 33.83 |

FFO 39, of 26 empty squares, took more than 60 s and was stopped. So
the host solves at 20 empty squares by default, in about a second.
Edax solves these much faster: its order of the moves, by a shallow
search, and its speed per node are what to look at when the solver
needs to be faster.

The size of the MEGA65 build: 47078 words of the 94176, with the
table's 16384. The deepest stacks, over a solve of 14 empty squares:
309 words of the wasm stack and 598 of the NICE stack.

Matches of the host build (`tools/match.py`, 6 random moves of an
opening, each played with both colors): at depth 4 against random
moves, 20 of 20, +47 discs a game; depth 6 against depth 2, 38 of 40.

## 2026-11-14: The speed of the move generator, at the start

Perft from the start, with the code of Stage 1.

On the host (`build/host/othello_fast`, gcc -O2), perft 11, 212258800
leaves, takes 0.83 s.

In niceemu (`build/nice/othello_text.bin`), in steps:

| Depth | Leaves | Steps |
|---:|---:|---:|
| 5 | 1396 | 270138 |
| 6 | 8200 | 1458632 |
| 7 | 55092 | 8495008 |

Perft counts the moves of the last level without making them, so the
cost is in the nodes above it: perft 7 visits 9913 of them, about 860
steps each. At 54 million steps a second, nice_chess's figure for the
MEGA65, that is some 63000 nodes a second.

The profile of perft 5 (nice_chess's `tools/prof.c`, on niceemu's
trace), of 291508 steps:

| Function | Steps | Share | A call |
|---|---:|---:|---:|
| `gen_moves` | 137895 | 47% | 435 |
| `gen_flips` | 75526 | 26% | 239 |
| `perft` | 26196 | 9% | 83 |
| `W.shl64` | 18960 | 7% | 10 |
| `W.zero1`, `W.zero0` | 18185 | 6% | 18 |
| `popcount` | 8052 | 3% | 33 |

What it shows, for later:

- `W.shl64` is a shift of a `uint64_t` by a variable count, 6 a call of
  `gen_flips`: clang turns the shifts by constants of the move's bit,
  `1 << sq`, into shifts of 1 by `sq + 1`, `sq + 8`, and so on.
- `W.zero1` and `W.zero0` clear the words of a function's locals in
  memory, as wasm's locals start at 0. `gen_moves` and `gen_flips`
  have many 64-bit locals.
- `gen_moves` does 6 steps in each of 8 directions on two words each.
  Kogge-Stone fills, or a loop on the halves, may do with fewer.
