# Release notes

The releases of nice_othello, newest first. Each section lists the
changes since the one before that a player or a user of the builds can
see.
[doc/HISTORY.md](https://github.com/MJoergen/nice_othello/blob/main/doc/HISTORY.md)
has the full story, and
[doc/RELEASE.md](https://github.com/MJoergen/nice_othello/blob/main/doc/RELEASE.md)
how a release is made.

A release is named `Vmajor.minor`, and its files are
`nice_othello-Vmajor.minor.tar.gz` and `SHA256SUMS`
([README.md](README.md#download)). Each section names the nice_cpu tag
the release is built with, and the NiceMonMEGA65 release it was
checked on.

## V1.0 (2026-11-22)

The first release: an Othello engine whose evaluation learned by
self-play, on the MEGA65's screen and keyboard, over its UART, in
niceemu, and on the host.

**What it needs**

- The MEGA65 builds need the monitor of NiceMonMEGA65. BASIC loads and
  runs them from the folder `NICE` of the SD card:
  `RUN "nice_othello-V1.0.bin"`. V1.0 was checked on a MEGA65 under
  NiceMonMEGA65 V2.4.1.
- The builds for niceemu need niceemu of nice_cpu V2.4, the window
  build for the windowed front end.
- The host build is for x86_64 Linux, linked statically.
- Built with nice_cpu V2.4.

**What it has**

- The engine: bitboards, iterative deepening and PVS with a
  transposition table, and an exact solver of the end of the game.
- An N-tuple evaluation, the 2×5 corner, the edge, and the diagonals,
  whose 75412 weights were learned by TD(λ) self-play in 3 million
  games. It scores 83% against the evaluation written by hand at
  depth 4.
- 5 levels, each a depth and a number of empty squares to solve at, so
  a level plays the same moves on the MEGA65, in niceemu, and on the
  host. Against Edax 4.7 at its level of the same number they score
  34 to 47%. The slowest moves on the MEGA65 take up to 13 seconds,
  at level 5.
- The windowed front end on the MEGA65's screen and keyboard, and the
  text front end over the UART, in niceemu, and on the host, which
  speaks NBoard after its command `nboard`.
