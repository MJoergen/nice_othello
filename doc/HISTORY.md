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

## Stage 2: a baseline engine (2026-11-14)

An evaluation written by hand (mobility, potential mobility, corners,
X- and C-squares), the search of the middle game, and the exact solver
of the end of the game. Scores are in hundredths of a disc, so that
the solver's results, in whole discs, and the evaluation's meet on one
scale, as the learned evaluation's will.

The search is PVS by iterative deepening, with a transposition table
and the moves ordered by the opponent's mobility after them. A node
whose depth reaches the end of the game is solved, so a search never
evaluates a position whose end it can see. Below a number of empty
squares (`solve N`: 20 on the host, 14 on NICE), the search solves the
root after 4 iterations, which order its moves.

The tests are the solver against a plain minimax, on random positions;
the FFO positions 1 to 38, against Edax's scores; and searches
of the text front end, the same on the host and in niceemu, node for
node. `tools/match.py` plays the engine against random moves, and
against itself at other limits.

## Stage 3: the N-tuple evaluation (2026-11-14)

`ntuple.c` has the 12 candidate patterns of the README, of which a
build uses those of `NT_MASK`, in `NT_STAGES` stages. The weights are
in RAM, where `engine_ntuple_weights` gives them to the caller; they
are 0 until the trainer of Stage 4 learns them. The text front end
chooses the evaluation with `evaluation hand|ntuple`. The hand-written
one stays the default.

The indexes are kept from black's view, so that a move changes only the
indexes of the squares it fills and flips: a list for each square of
its places and its power of 3 in each. The game's indexes are kept by
`board.c`, from scratch after an undo; the search updates them around
each move, only when the N-tuple evaluation is in use.

## Stage 4: the trainer (2026-11-14)

`tools/train.c` learns the weights of the N-tuple evaluation by self-play
on the host, with TD(λ) on afterstates: the program plays the move
whose afterstate it values best, or one at random with probability ε,
and after the game moves the value of each afterstate towards its
λ-return, the final disc difference at the end. It shares the engine's
move generator and the indexes of `ntuple.c`, in threads, each with
indexes of its own (`NT_TLS`). Each change of a weight goes, negated,
to the state with the colours swapped too, so that one game teaches
both colours.

It writes the weights in hundredths of a disc as 16-bit integers, in
the format of `src/ui/weights_file.h`, which the text front end on the
host loads with `weights FILE`; the platform reads the file, and NICE,
with no files, does not. All builds share the patterns and stages
(`NT_FLAGS`), so that the weights load in each. Putting them in the
NICE image is for Stage 6.

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
4. The transposition table's key is a hash of the two bitboards,
   computed at every node, not a Zobrist key kept up to date: a move
   flips a varying number of discs, and the hash is two multiplications
   by constants of 64 bits, which the translator writes inline.
5. The NICE builds and the host builds of the tests have tables of the
   same size, `TT_BITS` 13 (64 KB), so that a search visits the same
   nodes on both and the tests compare their whole output.
6. A pattern's places are the images of one place under all 8
   symmetries of the board, but for those that repeat another in the
   same order. A place that a symmetry maps onto itself, such as an
   edge or the 3×3 square of a corner, is so read twice, in two orders,
   with the same table: 92 places with all the patterns, where the
   README counts 50. The value of a position is then that of its
   images, with no tables to fold the indexes of symmetric states
   together, at the cost of the extra places. Folding instead is for
   Stage 5 to weigh.
7. The value of a position is that of black, negated for white, plus
   the weight of having the move: one table for both colours, from
   black's view.
8. The trainer's threads share the weights without locks, but each
   change of a weight is an atomic addition: with plain additions, the
   weights of a run of 8 threads grew without bound (LOG.md).
9. The trained weights are not in git yet: they are made by `make
   train`, in minutes. Stage 6, which puts them in the NICE image, will
   commit the table it makes.
