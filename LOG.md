# Log

Investigations made during the development (doc/HISTORY.md), newest first.

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
