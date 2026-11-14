# CLAUDE.md

nice_othello is an Othello engine in C for the NICE CPU (nice_cpu),
built like nice_chess. It runs in niceemu, on the MEGA65 under
NiceMonMEGA65's monitor, and on the host. See README.md for the design.

## Layout

- `src/`: the code; `src/README.md` says what each file does.
  `src/engine/` is the core: no I/O, no clock, no malloc, so the same
  files build for NICE and for the host. `src/platform/` (clock, key
  poll) and `src/ui/` (`text.c`) are around it.
- `tests/`: the inputs of the tests, `*.grep` (the lines of the output
  that are compared) and `*.expected`, and the test programs
  (`randgames.c`). `tools/runtest.sh` runs one test.
- `tools/`: `runtest.sh`, and `size.py`, which checks the size of the
  MEGA65 build against the budget (from nice_chess).
- `doc/HISTORY.md`: how the engine was built, by stages, and the
  decisions, after the fact. Plans and open items are GitHub issues,
  not files or TODO comments.
- `LOG.md`: investigations (measurements, options weighed), newest
  first, dated.
- `CONTRIBUTING.md`: building and the tests, for human contributors.
  Keep it, README.md, and `make help` in sync with the Makefile.
- `deps/` (fetched by `make deps`) and `build/` are not in git.
  `deps/nice_cpu-TAG` is nice_cpu at the pinned tag: the translator
  (`translator/nicebuild.py`), the runtime and libc, and the docs of the
  CPU and toolchain (`doc/ISA.md`, `doc/TOOLCHAIN.md`,
  `translator/WASM.md`).

## Commands

    make                # help: the targets
    make deps           # nice_cpu and niceemu into deps/
    make all            # build everything
    make test           # host and niceemu tests; run after every change
    make perft-deep     # perft 11 on the host
    make size           # the MEGA65 build against the 92K-word budget

`make board-text` needs `ETH=IP` or `PORT=DEVICE` and a NiceMonMEGA65
checkout (`NICEMON=DIR`). It is not in CI; run it only when the user
asks.

## Conventions

- The engine must keep working on NICE and on the host. The host build
  has asserts and `-DCHECK_STATE`, which checks the moves and flips of
  every perft node against the plain generator of `check.c`, so it
  catches engine bugs that the NICE builds, without asserts, do not.
- NICE builds are `-O 2 -D NDEBUG --no-bounds-checks`: an access out of
  bounds is not caught there; the host build's asserts are the check.
  `make size` must pass.
- Speed is measured in niceemu steps, which are deterministic. Report
  figures as steps, and as seconds on the board at about 54 million
  steps a second (nice_chess's figure, until measured for this program).
- Change a `*.expected` file only when the output changed on purpose,
  and say how in the commit message. The expected counts of perft are
  the published ones (OEIS A124004), and those of `randgames` in
  niceemu are the host's: never make them from a NICE run alone.
- Pins (`NICE_CPU_TAG`, the SHA-256 sum) are raised by hand,
  deliberately, not as a side effect.
- The docs are written in short, plain, factual sentences, and say what
  is true now. History goes in doc/HISTORY.md or LOG.md.
- Commit messages: a short subject (`Stage N: ...`, `doc: ...`,
  `CI: ...`), then the why.
