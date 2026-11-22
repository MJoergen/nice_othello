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

## V1.0

The first release. Built with nice_cpu V2.4. The date, and the
NiceMonMEGA65 release it was checked on, are added at the check on the
board (doc/RELEASE.md, steps 2 and 4).

- The engine: bitboards, an alpha-beta search with iterative
  deepening and a transposition table, an exact solver of the last
  empty squares, and an N-tuple evaluation whose weights were learned
  by TD(λ) self-play.
- 5 levels, by depth and the empty squares to solve at, so a level
  plays the same moves on the MEGA65, in niceemu, and on the host.
  Against Edax's level of the same number they score 34 to 47%.
- The windowed front end, on the MEGA65's screen and keyboard, and the
  text front end, over the UART, in niceemu, and on the host, which
  speaks NBoard after its command `nboard`.
