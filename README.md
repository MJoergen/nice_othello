# nice_othello

Play the game of Othello on the NICE processor.

This is a plan, and the start of its work. What is built so far is the
board, the move generator, an alpha-beta search with an evaluation
written by hand, the exact solver of the end of the game, and a text
front end, on the host, in niceemu, and on the MEGA65; and the N-tuple
evaluation, with a trainer that learns its weights by self-play on the
host. The weights are built into every build, and the N-tuple
evaluation is the one the engine plays with. The text front end has 5
levels, from a move in a moment to about 30 seconds a game on the
MEGA65. CONTRIBUTING.md says how to build it and run
the tests, and doc/HISTORY.md how far the work has come.

The engine will be for Othello
what [nice_chess](https://github.com/MJoergen/nice_chess) is for chess:
a program in C, translated for the NICE CPU by the toolchain of
[nice_cpu](https://github.com/MJoergen/nice_cpu), which runs in
niceemu, on the MEGA65 under the monitor of
[NiceMonMEGA65](https://github.com/MJoergen/NiceMonMEGA65), and on the
host. What is new is the evaluation: its weights are learned by the
program itself, by self-play, with temporal-difference (TD) learning.

## The idea

The strongest Othello programs do not use a deep neural network. The
evaluation of Logistello, which beat the world champion 6-0 in 1997, and
of Edax, one of the strongest today, is a sum of weights looked up in
tables, one table for each pattern of squares on the board. Such an
evaluation is called an N-tuple network. It is a neural network with no
hidden layer: the inputs are the states of a few squares at a time, and
the output is their weighted sum. The search does the rest.

That suits NICE well. An evaluation is a few dozen table lookups and
additions, in integers, with no multiplication. And the weights train
on a PC's CPU in hours, with no GPU.

So the plan is:

1. An N-tuple network for the evaluation, with weights in 16-bit
   integers.
2. The weights learned on the host by TD self-play.
3. An alpha-beta search with that evaluation, and an exact solver for
   the end of the game.

## The board

The board is two bitboards of 64 bits, one for each side. NICE is a
32-bit CPU, so each is two words. The legal moves and the discs that
a move flips are computed for all eight directions at once, with
shifts and masks. The same engine code builds for NICE and for the
host, as in nice_chess.

## The evaluation

A pattern is a set of squares, such as the edge, or the 3×3 square in
a corner. Each square is empty, mine, or the opponent's, so a pattern
of n squares has 3^n states, and a table of 3^n weights. The board
holds each pattern at several places: the edge four times, the corner
square four times, and so on. All places of a pattern share one table,
so that what is learned at one corner holds at the others.

The evaluation of a position, from the side to move, is the sum of the
weights of the states of each pattern at each of its places. The
indexes are kept up to date as discs are placed and flipped, so an
evaluation is the sum alone.

The candidate patterns, after Logistello's:

| Pattern | Squares | Weights | Places |
|---|---:|---:|---:|
| Corner 3×3 | 9 | 19,683 | 4 |
| Corner 2×5 | 10 | 59,049 | 8 |
| Edge and the two X-squares | 10 | 59,049 | 4 |
| Row 1 (the edge) | 8 | 6,561 | 4 |
| Row 2 | 8 | 6,561 | 4 |
| Row 3 | 8 | 6,561 | 4 |
| Row 4 | 8 | 6,561 | 4 |
| Diagonal of 8 | 8 | 6,561 | 2 |
| Diagonal of 7 | 7 | 2,187 | 4 |
| Diagonal of 6 | 6 | 729 | 4 |
| Diagonal of 5 | 5 | 243 | 4 |
| Diagonal of 4 | 4 | 81 | 4 |

A position early in the game is judged differently from one near its
end. So the game is split into stages by the number of discs, and each
stage has tables of its own, plus a weight for the side to move.

The tables must fit in memory. The monitor of NiceMonMEGA65 gives a
program 92K words, and a word holds two 16-bit weights. All the
patterns above make some 174K weights a stage, 87K words, which leaves
too little for the rest even with one stage. So the set of patterns and
the number of stages are chosen by measurement: the strength that each
adds, against the words it costs. A first target is some 40K words of
tables, for example 20K weights in each of 4 stages, and the rest for
the code, the search's tables, and the screen.

## Learning by self-play

The weights are learned on the host, in floating point, and then
rounded to 16-bit integers for NICE. The learning is TD(λ) on
afterstates:

- The program plays itself. In each position it makes each legal move,
  evaluates the position after it, and plays the best, or with a small
  probability ε a random move, so that it sees more positions.
- After each move, the evaluation of the previous position moves
  towards that of the new one, by a learning rate α. At the end of the
  game, the target is the final disc difference, which is known
  exactly.
- The games start from random positions a few moves into the game, so
  that they do not all follow the same lines.

A self-play game looks one move ahead, and an evaluation is a few dozen
lookups, so the host plays thousands of games a second on each core.
Published N-tuple players were trained on some hundreds of thousands to
a few million games, so a strong set of weights should take an
overnight run on a desktop CPU. That estimate is to be measured.

Each set of weights is checked by matches: against the set before it,
and against a fixed baseline, such as a simple evaluation of mobility
and corners. Only a set that wins is kept.

## The search

- Negamax with alpha-beta, iterative deepening, and a transposition
  table.
- The moves ordered by a shallow search, and by mobility: a move that
  leaves the opponent few moves first.
- In the last 15 to 20 empty squares, on the host, an exact solver: it
  plays out the game to its end, and knows the final disc count. On
  NICE, fewer, as its speed allows. It orders by parity and by the
  opponent's mobility, and stops at the first win when only the result
  is wanted.
- A pass, when a side has no legal move, and the end of the game, when
  neither has.

## Front ends and rating

As in nice_chess: a windowed front end for the MEGA65's screen and
keyboard, a text front end over the UART, and the engine on the host.
On the host, it speaks the NBoard protocol, which Othello GUIs use, so
that its strength can be rated in matches against Edax at its levels,
as nice_chess is rated against Stockfish.
