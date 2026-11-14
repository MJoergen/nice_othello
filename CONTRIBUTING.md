# Contributing

This file is for anyone who changes the repository: how to build it and
run the tests.

## Building and testing

Needs Python 3.10+, clang with the wasm32 target, `wasm-ld`, `llvm-ar`,
a C compiler for the host, `git`, and `curl`.

    make             # list the targets
    make deps        # nice_cpu V2.0 and niceemu V2.0 into deps/
    make all         # build/nice/ for niceemu, build/mega65/ for the MEGA65, build/host/ for the host
    make test        # the tests, on the host and in niceemu
    make perft-deep  # perft to depth 11 on the host, without the checks (a few seconds)
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

## On the MEGA65

    make board-text ETH=IP       # or PORT=/dev/ttyUSBn

loads the text front end under NiceMonMEGA65's monitor and runs it. It
needs a checkout of NiceMonMEGA65 (`NICEMON=DIR`, `../NiceMonMEGA65` by
default).
