# History

How nice_othello came to be, after the fact. What is true now is in the
README and `src/README.md`; open items are
[GitHub issues](https://github.com/MJoergen/nice_othello/issues).
Investigations made along the way are in [LOG.md](../LOG.md).

## The plan

The README of commit `9c424bc` gives the design: an N-tuple evaluation
whose weights are learned by TD self-play on the host, an alpha-beta
search, and an exact solver at the end of the game. The work is done in
stages, each of which ends with something that runs and is tested:

0. The project: the layout, the Makefile, and the tests of nice_chess.
1. The board, the move generator, and perft.
2. A baseline engine: a hand-written evaluation, the search, and the
   endgame solver.
3. The N-tuple evaluation, kept up to date as discs are placed and
   flipped.
4. The trainer: TD(λ) self-play on the host.
5. The patterns and stages, chosen by measurement within the budget.
6. NICE and the MEGA65: the weights in the image, the levels.
7. The front ends: text over the UART, and the MEGA65's screen.
8. NBoard, and a rating against Edax.

## Stages 0 and 1: the project, the board, and perft (2026-11-14)

The layout, the Makefile, the platform layer, and `tools/runtest.sh`
are nice_chess's, cut down to what the text front end needs. The
engine is new.

The board is two `uint64_t` bitboards, from the side to move. The
translator keeps a `uint64_t` in two words, and writes a shift by a
constant inline, so the move generator shifts by constants only: each
of the 8 directions is written out by a macro. Perft is that of
OEIS A124004: a pass counts as a move, and the end of the game as a
leaf.

## Decisions

1. nice_cpu is pinned to V2.0, the release nice_chess uses.
2. Black is X and white is O, and a position is 64 characters and the
   side to move, as in Edax and NBoard, so that positions can be passed
   between them.
3. The tests of the move generator are a plain generator (`check.c`),
   which the host build checks every node of perft against, and games
   of random moves to their end, which reach the positions perft from
   the start does not. The counts the NICE builds must match are the
   published ones and the host's.
