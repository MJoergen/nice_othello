# Log

Investigations made during the development (doc/HISTORY.md), newest first.

## 2026-11-15: The weights on NICE, and the levels, at Stage 6

The weights in the build: those of `make train` at Stage 5, 3000000
games, 83% against the hand-written evaluation at depth 4. With them,
FFO #1 (14 empties) solves in 51767 nodes instead of 81338, and the
moves of FFO #4 and #6 change to others of the same score.

The cost of the search on NICE, `go depth 7` from the start in
niceemu: 2593 nodes in 6118576 steps with the N-tuple evaluation, 2360
steps a node; 3145 nodes in 7850312 steps with the hand-written one,
2496 a node.

The time of a move by depth, in self-play of the NICE build from 10
openings of 6 random moves, the moves with more than 20 empty squares,
in steps, and the worst in seconds on the MEGA65 at 54 million steps a
second:

| Depth | Mean | Worst | Worst, seconds |
|---:|---:|---:|---:|
| 1 | 31214 | 49739 | 0.00 |
| 2 | 142660 | 313147 | 0.01 |
| 3 | 609156 | 1400610 | 0.03 |
| 4 | 1906176 | 4800807 | 0.09 |
| 5 | 6741030 | 29625550 | 0.55 |
| 6 | 20359388 | 77548389 | 1.44 |
| 7 | 84720443 | 348802818 | 6.46 |
| 8 | 216007450 | 1107536134 | 20.51 |

The levels, by the same games: the time of both sides together for a
game, and the worst move, which is the first that solves.

| Level | Depth | Solves at | A game | Worst midgame move | Worst solve |
|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 8 | 0.1 s | 0.00 s | 0.03 s |
| 2 | 2 | 10 | 0.2 s | 0.01 s | 0.07 s |
| 3 | 4 | 12 | 1.7 s | 0.09 s | 0.71 s |
| 4 | 6 | 14 | 15.7 s | 1.44 s | 3.26 s |
| 5 | 7 | 16 | 63.3 s | 6.46 s | 14.81 s |

Each level against the one below, on the host (`othello_fast`), 50
openings with both colours: 2 against 1, 86%; 3 against 2, 91%; 4
against 3, 78.5%; 5 against 4, 68.5%. Level 1 against random moves,
98%.

## 2026-11-14: The patterns and the stages, at Stage 5

`tools/variant.sh` builds the trainer and the engine for a choice of
patterns (`NT_MASK`) and stages, trains 3000000 games on 8 threads, and
plays the weights against the hand-written evaluation at depth 4, from
200 openings of 6 random moves, each with both colours: 400 games. The
machine was shared with other work throughout.

The budget: the MEGA65 build may take 94176 words. Its code is about
18000, so the linear memory may have 4 pages of 16384 words, 256 KB:
the stack 8 KB, the move stack 11 KB, the rest of the data about 4 KB,
the table 64 KB at `TT_BITS` 13 (32 KB at 12), and the weights the rest,
about 170 KB, 85000 weights (100000 with the smaller table).

| Name | Patterns | Stages | Weights | Places | Score at depth 4 |
|---|---|---:|---:|---:|---:|
| base1 | row1, row2, diagonals | 1 | 22924 | 52 | 60.0% |
| base2 | the same | 2 | 45848 | 52 | 62.4% |
| base4 | the same | 4 | 91696 | 52 | 61.4% |
| rows1 | rows 1 to 4, diagonals | 1 | 36046 | 68 | 58.5% |
| lean2 | 3×3 corner, row1, row2, diag8 | 2 | 78734 | 28 | 63.0% |
| lean2d7 | lean2 and diag7 | 2 | 83108 | 36 | 68.8% |
| corner1 | 3×3 corner, row1, row2, diagonals | 1 | 42607 | 60 | 71.8% |
| edge1 | 3×3 corner, edge and X-squares, diagonals | 1 | 88534 | 52 | 71.9% |
| c2x5_c3_norow | 3×3 and 2×5 corners, diagonals | 1 | 88534 | 52 | 53.6% |
| c2x5_lean | 2×5 corner, row1, diag8, diag7 | 1 | 74359 | 28 | 73.1% |
| c2x5_1 | 2×5 corner, row1, diagonals | 1 | 75412 | 52 | 84.8%, 76.1%, 75.5% |
| c2x5_r2 | c2x5_1 and row2 | 1 | 81973 | 60 | 80.0%, 81.1%, 77.4% |
| c2x5_c3 | c2x5_1 and the 3×3 corner | 1 | 95095 | 60 | 75.4%, 80.8%, 80.1% |
| full4 | all 12 | 4 | 695308 | 92 | 87.4% |

The three scores of a row are three trainings, with the seeds 1, 2, and
3. They differ by up to 9 points, so the training, in threads, is not
the same twice, and a single score is good to about ±5 points; the
match alone, of 400 games, to about ±2.5. With that:

- The 2×5 corner is worth the most: the sets with it score about 79%,
  those with only the 3×3 corner or the edge about 72%, and the first
  set about 61%. All 12 patterns in 4 stages, 7 times the budget, score
  87%.
- The stages add nothing that these matches show: base1, base2, and
  base4 are within the noise.
- Rows 3 and 4 add nothing either.
- c2x5_1, c2x5_r2, and c2x5_c3 are even. c2x5_c3_norow, at 54%, is
  one training; it may be a bad one.

c2x5_1 trained for 10000000 games scored 77.9%, as at 3000000: the
learning has reached what it reaches at α 0.0005 by then.

The cost on NICE, in steps a call, by nice_chess's `tools/prof.c` on
`go depth 5` in niceemu: `nt_eval` is about 11.5 steps a place and 23
more, and `nt_play` grows with the squares of the places.

| Places | Sets | `nt_eval` | `nt_play` |
|---:|---|---:|---:|
| 28 | lean, c2x5_lean | 367 | 104 to 142 |
| 36 | lean and diag7 | 459 | 154 |
| 52 | base, edge1, c2x5_1 | 643 | 175 to 182 |
| 60 | corner1, c2x5_r2, c2x5_c3 | 735 | 181 to 187 |
| 68 | rows1 | 827 | 272 |

The choice: c2x5_1, `NT_MASK` 0xF8A in 1 stage. It is as strong as any
set that fits, and the cheapest of them on NICE, in steps and in words:
75412 weights, 151 KB, beside a table of 64 KB. The MEGA65 build then
needs 82689 words of the 94176. c2x5_lean, with half the places, may be
as good on the board, where a cheaper evaluation searches deeper in the
same time; that is to measure on NICE, with the weights in the image.

## 2026-11-14: The first training, at Stage 4

The patterns of all the builds (`NT_MASK` 0xF98: row1, row2, and the
diagonals, 52 places, 45848 weights in 2 stages), `tools/train.c` with
λ 0.5, ε 0.1, and up to 6 random moves at the start of a game. The
score is that of the trainer's own match: the learned evaluation
against the hand-written one, both by the afterstates alone (1 ply),
200 games. The machine was shared with other work, at a load of 13 to
75 on its 20 threads, so the speeds are low bounds.

The rate α, and the threads:

| Threads | α | Games | Score along the run | Largest weight |
|---:|---:|---:|---|---:|
| 1 | 0.0005 | 1000000 | 78%, 85% at 300000, 87% | 10 |
| 1 | 0.001 | 300000 | 83%, 88% | |
| 8 | 0.0005 | 300000 | 82%, 85%, then 40% | |
| 8 | 0.001 | 300000 | 44%, 7% | |
| 8 | 0.0002 | 3000000 | 82% at 250000, 73%, 33%, 6% | 8123 |

With 8 threads, the weights grew without bound, at every rate tried,
sooner at the higher ones; with one thread, they did not. The threads
shared only the weights, and added to them with plain `+=`, so they lost
some of each other's changes. Each change of a weight goes, negated, to
the state with the colours swapped too, and a pair whose two changes do
not both get lost no longer cancels: the antisymmetry drifts, most on
the weights that every game changes (the weight of the move, the states
of the openings), and the bootstrapping of TD then feeds the error.
With each change an atomic addition (a compare and exchange), 8 threads
at α 0.0005 kept the score at 84 to 88% to 1500000 games, the largest
weight at 10, as one thread did. The speed was about 29000 games a
second, as without the atomics; one thread, about 2000 to 3000 (with
the load).

`make train`, 2000000 games, 8 threads, α 0.0005: 32000 games a second,
the score at 85 to 89% from 250000 games on. Its weights, in the
engine, against the hand-written evaluation, `tools/match.py` with 100
openings of 6 random moves, each played with both colours:

| Depth | Score |
|---:|---:|
| 2 | 138.5 of 200, 69% |
| 4 | 120.5 of 200, 60% |
| 6 | 116.5 of 200, 58% |

The learned evaluation is the better one, by less as the search goes
deeper. These patterns are few: the corners, and rows 3 and 4, are not
in them.

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
