# Contributing

This file is for anyone who changes the repository: how to build it and
run the tests.

## Building and testing

Needs Python 3.10+, clang with the wasm32 target, `wasm-ld`, `llvm-ar`,
a C compiler for the host, `git`, and `curl`. `make dist` links the
host build statically, which needs glibc's static library: `libc6-dev`
has it on Debian and Ubuntu, `glibc-static` on Fedora.

    make             # list the targets
    make deps        # nice_cpu V2.4, niceemu V2.4, and the FFO positions into deps/
    make all         # build/nice/ for niceemu, build/mega65/ for the MEGA65, build/host/ for the host
    make test        # the tests, on the host and in niceemu, and of tools/release.py
    make perft-deep  # perft to depth 11 on the host, without the checks (a few seconds)
    make ffo-deep    # FFO positions 20 to 38 on the host (about a minute)
    make match-random   # the host build at depth 4 against random moves (20 games)
    make train       # learn the N-tuple weights by self-play into build/train/weights.bin
    make match-hand  # the N-tuple evaluation with those weights against the hand-written one
    make weights     # make those weights the builds' (src/engine/ntuple_weights.bin)
    make edax        # fetch and build Edax at its pinned commit, with its weights
    make rate        # the 5 levels against Edax's levels 1 to 5 (a few minutes)
    make size        # the size of the MEGA65 build, against the 92K words the monitor leaves it
    make dist        # the archive of a release, and SHA256SUMS, in build/dist/

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
- `ntcheck`: `tests/ntcheck.c` sets the N-tuple weights at random, and
  checks, on positions of random games, the indexes against those from
  scratch, that the value of a position is that of its 8 images under
  the symmetries of the board, that a search of depth 1 finds the best
  of the moves each evaluated from scratch, and that a search leaves
  the indexes as it found them. 200 positions on the host, 20 in
  niceemu. perft and `randgames` check the indexes too.
- `train`: the selftest of `tools/train.c`: 2000 games of training, and
  then the values are antisymmetric in the colours, and the weights it
  writes give the engine the trainer's values, to the rounding.
- `nboard`: the NBoard protocol of the text front end, through
  `tests/nboard.input`: games in GGF, go, hint, ping, and errors.
- `screen`, `screen_game`: the windowed front end in niceemu, by the
  keys of `tests/*.keys`, then the moves it logs and the screen at the
  end, with its colors. `screen` tries each key; in `screen_game` the
  engine plays a whole game, with a pass, by the key M. Its moves are
  those of the text front end's `go` at the same level, search for
  search.
- `tools-test`: the tests of `tools/release.py`, in
  `tools/test_release.py`.
- `ffo1`: `tests/ffosuite.c` solves the FFO positions 1 to 19 (3 in
  niceemu), of 14 to 16 empty squares, which must give the published
  scores. `make ffo-deep` solves 20 to 38, of up to 24.

`tools/match.py` plays two programs, or one and random moves, from
random openings, each with both colors. A program is the text front
end, or one that speaks NBoard (`nboard:COMMAND`) or GTP
(`gtp:COMMAND`, as Edax does), or the text front end on the board
(`board:FILE.bin`). `tools/rate.sh`, which `make rate`
runs, plays the levels against Edax's and gives the difference of Elo.
Edax is not in `make deps`: `make edax` needs git, a C compiler, and
7z. `tools/variant.sh NAME MASK
STAGES` builds, trains, and measures another choice of the N-tuple
patterns and stages, in `build/variants/NAME/`.

## On the MEGA65

    make board-text ETH=IP       # or PORT=/dev/ttyUSBn
    make board-screen ETH=IP

load the text front end, or the windowed one, under NiceMonMEGA65's
monitor and run it. The keys of the windowed front end are at the top
of `src/ui/screen.c`, and on its screen. Both need a checkout of NiceMonMEGA65 (`NICEMON=DIR`, `../NiceMonMEGA65` by
default).

    make board-test ETH=IP
    make board-match ETH=IP

run the tests of the MEGA65 builds on the board, through
`tools/board.py`, which must give what they give in niceemu, and play
two games of the board against the host build.

The front ends show `VERSION`: the release tag on a tagged commit, and
what `git describe` says between tags. `build/gen/version.c` holds it.
`tools/runtest.sh` replaces it with `VERSION` in the output, so the
`*.expected` files do not depend on it.

## Releases

[doc/RELEASE.md](doc/RELEASE.md) is how to make a release, with
`tools/release.py` and `make release-check`, which runs `make
board-test` and `make board-match` on the MEGA65. The push of a tag
starts [release.yml](.github/workflows/release.yml), which builds the
archive and creates the GitHub release.
[RELEASE_NOTES.md](RELEASE_NOTES.md) has what changed in each release.
