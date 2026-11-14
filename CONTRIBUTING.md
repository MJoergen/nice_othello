# Contributing

This file is for anyone who changes the repository: how to build it and
run the tests.

## Building and testing

Needs Python 3.10+, clang with the wasm32 target, `wasm-ld`, `llvm-ar`,
a C compiler for the host, `git`, and `curl`.

    make             # list the targets
    make deps        # nice_cpu V2.0, niceemu V2.0, and the FFO positions into deps/
    make all         # build/nice/ for niceemu, build/mega65/ for the MEGA65, build/host/ for the host
    make test        # the tests, on the host and in niceemu
    make perft-deep  # perft to depth 11 on the host, without the checks (a few seconds)
    make ffo-deep    # FFO positions 20 to 38 on the host (about a minute)
    make match-random   # the host build at depth 4 against random moves (20 games)
    make size        # the size of the MEGA65 build, against the 92K words the monitor leaves it

To use local copies of nice_cpu and niceemu instead, pass `NICE_CPU=DIR`
and `NICEEMU=PATH` to make.

## The tests

`make test` runs, on the host, in niceemu, and in niceemu as the
MEGA65 build after a stand-in for the monitor:

- `text`: the text front end, through the commands of
  `tests/text.input`.
- `perft9` on the host and `perft7` in niceemu: perft from the start,
  against the published counts (OEIS A124004). On the host every node is
  checked against the plain move generator of `src/engine/check.c`.
- `randgames`: `tests/randgames.c` plays games of random moves to
  their end, 10000 on the host and 200 in niceemu, and checks every
  position against the plain move generator. The games are the same on
  both, so the counts it prints must be the same too.
- `search`: searches of the text front end, through
  `tests/search.input`. The host build of the tests has the NICE
  builds' transposition table (`TT_BITS`), so both visit the same nodes
  and print the same lines.
- `solvecheck`: `tests/solvecheck.c` solves positions of random games
  with a few empty squares, 200 of 10 on the host and 20 of 8 in
  niceemu, and compares the score, the move, and the win, draw, or loss
  with a plain minimax.
- `ffo1`: `tests/ffosuite.c` solves the FFO positions 1 to 19 (3 in
  niceemu), of 14 to 16 empty squares, which must give the published
  scores. `make ffo-deep` solves 20 to 38, of up to 24.

`tools/match.py` plays two programs, or one and random moves, from
random openings, each with both colors.

## On the MEGA65

    make board-text ETH=IP       # or PORT=/dev/ttyUSBn

loads the text front end under NiceMonMEGA65's monitor and runs it. It
needs a checkout of NiceMonMEGA65 (`NICEMON=DIR`, `../NiceMonMEGA65` by
default).
