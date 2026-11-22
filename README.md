# nice_othello

[![CI](https://github.com/MJoergen/nice_othello/actions/workflows/test.yml/badge.svg)](https://github.com/MJoergen/nice_othello/actions/workflows/test.yml)

An Othello engine for the NICE system, whose evaluation learned to play
by playing itself.

It runs on the MEGA65 with the board on the screen and moves entered by
the keyboard, and as a text front end over the UART, in niceemu, and on
the host, where it speaks NBoard. It is a program in C, translated for
the NICE CPU by the toolchain of
[nice_cpu](https://github.com/MJoergen/nice_cpu), as
[nice_chess](https://github.com/MJoergen/nice_chess) is.
[doc/HISTORY.md](https://github.com/MJoergen/nice_othello/blob/main/doc/HISTORY.md)
tells how it was built.

## Download

Each [release](https://github.com/MJoergen/nice_othello/releases) has
`nice_othello-TAG.tar.gz`, where TAG is the release, such as `V1.0`,
and `SHA256SUMS`. [RELEASE_NOTES.md](RELEASE_NOTES.md) says what
changed in each, and what it needs. The archive's directory has:

- `mega65/`: both front ends for the MEGA65, under the monitor of
  NiceMonMEGA65: `nice_othello-TAG.bin`, the windowed front end, and
  `nice_othello-text-TAG.bin`, the text front end.
- `niceemu/`: both front ends for niceemu:
  `nice_othello-niceemu-TAG.bin` and
  `nice_othello-text-niceemu-TAG.bin`. They are built for niceemu
  alone, and do not run under the MEGA65's monitor.
- `linux-x86_64/nice_othello`: the text front end on the host, linked
  statically. It speaks NBoard after its command `nboard`.

The front ends show their version: the banner of the text front end,
and the top of the windowed front end's panel.

## On the MEGA65

`nice_othello-TAG.bin` runs on the MEGA65 under the monitor of
[NiceMonMEGA65](https://github.com/MJoergen/NiceMonMEGA65). BASIC's
`LOAD` loads it from a FAT32 SD card, so the board needs no host.

On a PC, make the folder `NICE` in the root of the card and copy
`mega65/nice_othello-TAG.bin` from the release into it. Put the card
in the MEGA65 and type at BASIC's `READY.`, with the release's tag,
such as:

    RUN "nice_othello-V1.0.bin"

`nice_othello-text-TAG.bin` is the text front end, on the monitor's
console: the UART, or the Ethernet port (NiceMonMEGA65's README says
how to reach it).

## Playing

The board is on the left of the 80×25 screen; a dot marks each square
where you may move. Move with the cursor keys and RETURN, or type a
move such as `d3` and RETURN. Space is RETURN, INST/DEL erases a typed
move, and ESC drops it. A side with no move passes by itself.

The other keys are on the screen:

- M: the engine makes your move.
- U: take back your last move, and the engine's reply.
- S: switch sides: the engine plays the side to move.
- N: a new game.
- \+ and −: the level, 1 to 5.
- ESC, while the engine thinks: make it move now.
- Q: clear the screen and return to BASIC.

You are black, and move first. The game starts at level 1, the
weakest. A level is a depth of the search, and a number of empty
squares from which the engine plays the end of the game perfectly:

| Level | Depth | Solves at | Worst move in the middle game | Worst move that solves | Against Edax's level |
|------:|------:|----------:|------:|------:|-----:|
| 1 | 1 | 8 | 0.00 s | 0.03 s | 41% |
| 2 | 2 | 10 | 0.01 s | 0.06 s | 34% |
| 3 | 4 | 12 | 0.08 s | 0.63 s | 47% |
| 4 | 6 | 14 | 1.3 s | 2.9 s | 44% |
| 5 | 7 | 16 | 5.7 s | 13 s | 42% |

The times are the worst moves of games of self-play, measured in
niceemu ([LOG.md](https://github.com/MJoergen/nice_othello/blob/main/LOG.md)),
at the 61 million steps a second that the MEGA65 runs this program.
Most moves take much less. The last column is the score against Edax
4.7 at its level of the same number, which searches less deep, by
`make rate`. A level plays the same moves on the MEGA65, in niceemu,
and on the host, so its strength does not depend on the machine.

## The text front end

The text front end takes one command a line. `help` lists them. The
main ones:

- `new`, `show`, `moves`, `undo`.
- `d3`, or `move d3`, or `pass`: make a move.
- `pos POSITION`: set up a position: 64 characters of `X` (black),
  `O` (white), and `-`, from a1 to h8, row by row, a space, and the
  side to move.
- `go [depth N] [nodes N] [movetime MS] [wld]`: the engine searches,
  prints a line for each depth, and makes its move. Without limits it
  searches to the depth of the level, or for 5 seconds on the MEGA65,
  1 on the host, without one.
- `level N`: the level of `go` without limits, 1 to 5, or 0 for none.
- `solve N`: play the end of the game perfectly from N empty squares.
- `eval`, `evaluation ntuple|hand`: the evaluation, and which one.
- `nboard`: speak the NBoard protocol from then on.
- `perft N`, `divide N`: count the leaves of the tree of moves.
- `quit`.

## In niceemu

The front ends for niceemu run in the niceemu of nice_cpu's
[V2.4 release](https://github.com/MJoergen/nice_cpu/releases/tag/V2.4):
`niceemu-V2.4-x86_64-linux.tar.gz` runs the text front end, and the
window build, `niceemu-window-V2.4-x86_64-linux.tar.gz`, runs both.
From the archive:

    niceemu --step-time niceemu/nice_othello-text-niceemu-TAG.bin
    niceemu --window --step-time niceemu/nice_othello-niceemu-TAG.bin

`--step-time` makes niceemu's clock count steps rather than the host's
time, so a run is the same every time. `go` without limits and without
a level then takes 300 million steps.

## On the host

`linux-x86_64/nice_othello` has the same commands at a terminal. After
`nboard` it speaks the NBoard protocol, so an Othello GUI that speaks
NBoard can play it: it takes `set depth N`, `set game`, `move`, `go`,
`hint`, and `ping`. It has a larger transposition table than the
builds for NICE, and without a level its search solves the end of the
game from 20 empty squares, where theirs solves from 14.

## Building

[CONTRIBUTING.md](CONTRIBUTING.md) says how to build it and run the
tests, and [src/README.md](src/README.md) what each source file does.

## How it works

The strongest Othello programs do not use a deep neural network. The
evaluation of Logistello, which beat the world champion 6-0 in 1997, and
of Edax, one of the strongest today, is a sum of weights looked up in
tables, one table for each pattern of squares on the board. Such an
evaluation is called an N-tuple network: a neural network with no
hidden layer, whose inputs are the states of a few squares at a time.
That suits NICE: an evaluation is a few dozen table lookups and
additions, in integers. And the weights train on a PC's CPU, with no
GPU.

### The board

The board is two bitboards of 64 bits, one for each side; NICE is a
32-bit CPU, so each is two words. The legal moves and the discs that a
move flips are computed for all eight directions at once, with shifts
and masks.

### The evaluation

A pattern is a set of squares, such as the edge, or a 2×5 block in a
corner. Each square is empty, mine, or the opponent's, so a pattern of
n squares has 3^n states, and a table of 3^n weights. The board holds
each pattern at several places, which share one table, so that what is
learned at one corner holds at the others. The evaluation of a
position, for the side to move, is the sum of the weights of the states
of each pattern at each of its places. The indexes are kept up to date
as discs are placed and flipped, so an evaluation is the sum alone.

The builds use the 2×5 corner (8 places), the edge (4), and the
diagonals of 4 to 8 squares: 75412 weights of 16 bits, in one stage
for the whole game. The MEGA65's monitor gives a program 92K words, so
the patterns were chosen by measurement: the strength each adds,
against the words it costs (doc/HISTORY.md, LOG.md).

### Learning by self-play

The weights are learned on the host by `tools/train.c`, with TD(λ) on
afterstates. The program plays itself: in each position it plays the
move whose position after it it values best, or with a small
probability a random one. After the game, the value of each position
moves towards that of the positions after it, and at the end towards
the final disc difference. The games start from random openings. The
weights of the builds come from 3 million games, minutes on a desktop
CPU in 8 threads, and beat the evaluation written by hand, `evaluation
hand`: they score 83% against it at depth 4.

### The search

- Iterative deepening, and PVS with a transposition table, the moves
  ordered by the mobility they leave the opponent.
- In the last empty squares, an exact solver, which plays out the game
  to its end, or with `wld` finds only whether it is won, drawn, or
  lost.
- A pass, when a side has no legal move, and the end of the game, when
  neither has.

## License

MIT, in [LICENSE](LICENSE).
