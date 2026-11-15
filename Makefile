# nice_othello: an Othello engine for the NICE system.
#
# The NICE toolchain comes from nice_cpu at a pinned release tag.
# make deps fetches it, and niceemu, into deps/. NICE_CPU=DIR and
# NICEEMU=PATH use local copies of nice_cpu and niceemu instead.
# make help lists the targets.

NICE_CPU_TAG    := V2.0
NICEEMU_SHA256  := 7726cfdf6c86513d156ed75e52c287a7d88ef0c82c7bb49fa02558dde0cf7cf1
# Edax at a pinned commit (4.7): the FFO endgame positions, with their
# exact scores, from its problem/, and for make edax, its source, and
# the evaluation weights of its release v4.4, which its README names.
EDAX_COMMIT     := 786421f355219ade872ce7285c1cab1eaf67836a
FFO1_SHA256     := 898a13889df1cc83ede69b014f9df40650af6697f47cd96b4a0fb494ca404105
FFO20_SHA256    := b522fcdfd36811d5e07b56dc531e2d6272ec370caa45638e63f134e878558c25
EDAX_EVAL_SHA256 := 31a9fede7ae9a62bd3a78b2ba457d276d2421b15d3d85b6f8b9a9ba7beddf395

NICE_CPU ?= deps/nice_cpu-$(NICE_CPU_TAG)
NICEEMU  ?= deps/niceemu-$(NICE_CPU_TAG)-x86_64-linux/niceemu
NICEMON  ?= ../NiceMonMEGA65
PYTHON   ?= python3
CC       ?= cc
# The version the front end shows: the release tag on a tagged commit,
# such as V1.0, and git describe's name of the commit between tags.
# tools/runtest.sh replaces it with VERSION in the output, so the tests
# do not depend on it.
VERSION  ?= $(shell git describe --tags --match 'V[0-9]*' --always --dirty 2>/dev/null || echo unknown)
export NICEEMU NICEMON VERSION

# How to reach the board: ETH=IP or PORT=DEVICE, as NiceMonMEGA65's tools
# take them. Only the board targets need one.
BOARDLINK = $(if $(ETH),--eth $(ETH),$(if $(PORT),--port $(PORT),$(error ETH=IP or PORT=DEVICE names the board)))

NICEBUILD   = $(PYTHON) $(NICE_CPU)/translator/nicebuild.py
RELEASE_URL = https://github.com/MJoergen/nice_cpu/releases/download/$(NICE_CPU_TAG)
NICEEMU_TGZ = niceemu-$(NICE_CPU_TAG)-x86_64-linux.tar.gz
EDAX_RAW    = https://raw.githubusercontent.com/abulmo/edax-reversi/$(EDAX_COMMIT)/problem
EDAX_GIT    = https://github.com/abulmo/edax-reversi.git
EDAX_EVAL   = https://github.com/abulmo/edax-reversi/releases/download/v4.4/eval.7z
# Edax, built by make edax for the CPU EDAX_ARCH, as its Makefile names
# them (x86-64, x86-64-v2, x86-64-v3, ...). match.py plays it by GTP, on
# one thread, without its book.
EDAX_ARCH  ?= x86-64-v3
EDAX        = deps/edax/bin/lEdax-$(EDAX_ARCH)
EDAX_GTP    = gtp:$(EDAX) -gtp -n 1 -book-usage off -ponder off -eval-file deps/edax/data/eval.dat
FFO1        = deps/ffo/fforum-1-19.obf
FFO20       = deps/ffo/fforum-20-39.obf
# The first 3 positions of FFO1, for niceemu (about 300 million steps).
FFO1_NICE   = build/test/ffo1-3.obf

# The MEGA65 gives a program under the monitor the words 0x8000 to
# 0x1EFFF, 92K, for its image, its linear memory, and the NICE stack
# (NiceMonMEGA65 V1.2.1, as in nice_chess).
BUDGET = 94176

ENGINE  = $(wildcard src/engine/*.c)
# The headers, and the Makefile, whose flags every build takes, so that
# a change of the flags rebuilds.
HEADERS = $(wildcard src/engine/*.h) src/platform/platform.h src/ui/version.h src/ui/weights_file.h src/ui/levels.h Makefile \
          build/gen/weights.inc
# The version, for the front end (VERSION).
VERSIONC = build/gen/version.c

# The transposition table: 1 << TT_BITS entries of 8 bytes. The NICE
# builds and the host builds of the tests have the same, so that their
# searches visit the same nodes and print the same; the fast host build
# has a larger one.
TT_BITS      = 13
TT_BITS_FAST = 20

# The patterns of the N-tuple evaluation (NT_MASK, a bit for each of
# nt_defs in src/engine/ntuple.c) and its stages. All the builds have
# the same, so that the weights of the trainer load in each. 0xF8A is
# the 2x5 corner, row1, and the diagonals: 75412 weights in 1 stage,
# chosen by the measurements of LOG.md (2026-11-14, Stage 5).
NT_MASK   = 0xF8A
NT_STAGES = 1
# The weights of the builds: src/engine/ntuple_weights.bin, which
# make weights copies from a training (WEIGHTS), as C, by mkweights.py.
NT_WEIGHTS_BIN = src/engine/ntuple_weights.bin
NT_WEIGHTS_INC = build/gen/weights.inc
NT_FLAGS = -D NT_MASK=$(NT_MASK) -D NT_STAGES=$(NT_STAGES) -D 'NT_WEIGHTS="weights.inc"' -I build/gen

# NICE: optimized for speed, without asserts, and without the bounds
# checks of wasm's memory accesses, which the host builds' asserts and
# CHECK_STATE stand in for. --trust-alignment leaves out the alignment
# test of each access of 4 bytes, and --byte-windows reaches the memory
# through the byte windows of nice_cpu's SYSTEM.md §9 (niceemu and
# NiceMonMEGA65 from V2.0 on), as in nice_chess. The memory is four
# pages, 256 KB, for the table's 64 KB, the weights' 151 KB, the move
# stack, and the stack; a fifth page would not fit the board's budget.
NICEFLAGS = -O 2 -D NDEBUG -D TT_BITS=$(TT_BITS) $(NT_FLAGS) --no-bounds-checks --trust-alignment \
            --byte-windows --stack 8192 --memory 262144 --symbols --keep
# The MEGA65: the same, built for the monitor, which loads it at 0x8000.
MONFLAGS = $(NICEFLAGS) --monitor

# The host: with asserts, and a check of the moves and the flips of
# every node of perft against the plain generator of check.c.
HOSTFLAGS = -O2 -W -Wall -Wextra -DCHECK_STATE -DTT_BITS=$(TT_BITS) $(NT_FLAGS)
# The host, fast: without asserts and the check, and with a larger table.
FASTFLAGS = -O2 -W -Wall -Wextra -DNDEBUG -DTT_BITS=$(TT_BITS_FAST) $(NT_FLAGS)

# The trainer (tools/train.c): its games, threads, and other options,
# and the weights it writes, and that make match-hand plays.
TRAIN_GAMES   ?= 3000000
TRAIN_THREADS ?= 8
TRAIN_ARGS    ?=
WEIGHTS       ?= build/train/weights.bin

# The random games of tests/randgames.c, on the host and in niceemu.
RANDGAMES_HOST = 10000
RANDGAMES_NICE = 200
# The positions of tests/solvecheck.c, and their empty squares.
SOLVECHECK_HOST = 200 10
SOLVECHECK_NICE = 20 8
# The positions of tests/ntcheck.c.
NTCHECK_HOST = 200
NTCHECK_NICE = 20


.PHONY: FORCE help all test host-test nice-test perft-deep ffo-deep match-random train match-hand weights size deps edax rate clean distclean board-text board-screen
help:
	@echo "nice_othello: an Othello engine for the NICE system"
	@echo ""
	@echo "  make help          show this text (the default)"
	@echo "  make deps          fetch nice_cpu $(NICE_CPU_TAG), niceemu $(NICE_CPU_TAG), and the FFO positions into deps/"
	@echo "  make all           build the text front end, for niceemu, for the MEGA65, and for the host"
	@echo "  make test          the tests, on the host and in niceemu (about a minute)"
	@echo "  make perft-deep    perft to depth 11 on the host, without the checks (a few seconds)"
	@echo "  make ffo-deep      solve FFO positions 20 to 38 on the host (about a minute)"
	@echo "  make match-random  the host build at depth 4 against random moves (20 games)"
	@echo "  make train         learn the N-tuple weights by self-play into WEIGHTS (build/train/weights.bin)"
	@echo "                     (TRAIN_GAMES=$(TRAIN_GAMES), TRAIN_THREADS=$(TRAIN_THREADS), TRAIN_ARGS= more options of tools/train.c)"
	@echo "  make match-hand    the N-tuple evaluation with WEIGHTS against the hand-written one, at depth 4 (100 games)"
	@echo "  make weights       copy WEIGHTS into $(NT_WEIGHTS_BIN), the weights of the builds"
	@echo "  make size          the size of the MEGA65 build, against the board's budget"
	@echo "  make clean         remove build/"
	@echo "  make distclean     remove build/ and deps/"
	@echo ""
	@echo "On the MEGA65, under NiceMonMEGA65's monitor, with ETH=IP (the board's"
	@echo "Ethernet port) or PORT=/dev/ttyUSBn (its debug UART), and the tools of a"
	@echo "checkout of NiceMonMEGA65 (NICEMON=DIR, ../NiceMonMEGA65 by default):"
	@echo ""
	@echo "  make edax          fetch and build Edax, and its weights, into deps/edax"
	@echo "  make rate          the levels against Edax's (RATE_PAIRS, RATE_OPENINGS), a few minutes"
	@echo "  make board-text    load and run the text front end, then talk to it over the line"
	@echo "  make board-screen  load and run the windowed front end, for the screen and keyboard"
	@echo ""
	@echo "NICE_CPU=DIR and NICEEMU=PATH use local copies of nice_cpu and niceemu."

# The NICE builds depend on the toolchain: its directory and commit, in
# a file that changes only when they do, so that a new pin or another
# NICE_CPU=DIR builds them again.
TOOLCHAIN = build/toolchain.txt
$(TOOLCHAIN): FORCE | $(NICE_CPU)
	@mkdir -p $(@D)
	@t="$(abspath $(NICE_CPU)) $$(git -C $(NICE_CPU) rev-parse HEAD 2>/dev/null)"; \
	  [ "$$t" = "$$(cat $@ 2>/dev/null)" ] || echo "$$t" > $@

$(NT_WEIGHTS_INC): $(NT_WEIGHTS_BIN) tools/mkweights.py Makefile
	@mkdir -p $(@D)
	$(PYTHON) tools/mkweights.py $< $(NT_MASK) $(NT_STAGES) $@

$(VERSIONC): FORCE
	@mkdir -p $(@D)
	@t='const char nice_othello_version[] = "$(VERSION)";'; \
	  [ "$$t" = "$$(cat $@ 2>/dev/null)" ] || echo "$$t" > $@

FORCE:

all: build/nice/othello_text.bin build/nice/othello_screen.bin build/mega65/othello_text.bin \
     build/mega65/othello_screen.bin build/host/othello_text build/host/othello_fast

build/nice/othello_text.bin: $(ENGINE) $(HEADERS) src/ui/text.c src/platform/nice.c $(VERSIONC) $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/text.c src/platform/nice.c

build/mega65/othello_text.bin: $(ENGINE) $(HEADERS) src/ui/text.c src/platform/nice.c $(VERSIONC) $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(MONFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/text.c src/platform/nice.c

build/nice/othello_screen.bin: $(ENGINE) $(HEADERS) src/ui/screen.c src/platform/nice.c $(VERSIONC) $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/screen.c src/platform/nice.c

build/mega65/othello_screen.bin: $(ENGINE) $(HEADERS) src/ui/screen.c src/platform/nice.c $(VERSIONC) $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(MONFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/screen.c src/platform/nice.c

build/nice/randgames.bin: $(ENGINE) $(HEADERS) tests/randgames.c $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -D RANDGAMES=$(RANDGAMES_NICE) -o $@ $(ENGINE) tests/randgames.c

# nice_cpu's stand-in for a monitor at 0, which runs a program at 0x8000
# as the monitor does, so that the MEGA65 build runs in niceemu too,
# joined after it (a .bin file is address and word pairs).
build/nice/monstub.bin: $(NICE_CPU)/runtime/test/monitor.nasm $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(PYTHON) $(NICE_CPU)/assembler/niceasm.py -o $@ $<

build/test/mon_%.bin: build/nice/monstub.bin build/mega65/%.bin
	@mkdir -p $(@D)
	cat $^ > $@

build/host/othello_text: $(ENGINE) $(HEADERS) src/ui/text.c src/platform/host.c $(VERSIONC)
	@mkdir -p $(@D)
	$(CC) $(HOSTFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/text.c src/platform/host.c

build/host/othello_fast: $(ENGINE) $(HEADERS) src/ui/text.c src/platform/host.c $(VERSIONC)
	@mkdir -p $(@D)
	$(CC) $(FASTFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/text.c src/platform/host.c

build/host/randgames: $(ENGINE) $(HEADERS) tests/randgames.c
	@mkdir -p $(@D)
	$(CC) $(HOSTFLAGS) -o $@ $(ENGINE) tests/randgames.c

build/host/solvecheck: $(ENGINE) $(HEADERS) tests/solvecheck.c
	@mkdir -p $(@D)
	$(CC) $(HOSTFLAGS) -o $@ $(ENGINE) tests/solvecheck.c

build/host/train: $(ENGINE) $(HEADERS) tools/train.c
	@mkdir -p $(@D)
	$(CC) $(FASTFLAGS) -DNT_TLS=_Thread_local -pthread -o $@ $(ENGINE) tools/train.c -lm

build/host/ntcheck: $(ENGINE) $(HEADERS) tests/ntcheck.c
	@mkdir -p $(@D)
	$(CC) $(HOSTFLAGS) -o $@ $(ENGINE) tests/ntcheck.c

build/nice/ntcheck.bin: $(ENGINE) $(HEADERS) tests/ntcheck.c $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -D NTCHECK_POSITIONS=$(NTCHECK_NICE) -o $@ $(ENGINE) tests/ntcheck.c

build/host/ffosuite: $(ENGINE) $(HEADERS) tests/ffosuite.c
	@mkdir -p $(@D)
	$(CC) $(HOSTFLAGS) -o $@ $(ENGINE) tests/ffosuite.c

build/host/ffosuite_fast: $(ENGINE) $(HEADERS) tests/ffosuite.c
	@mkdir -p $(@D)
	$(CC) $(FASTFLAGS) -o $@ $(ENGINE) tests/ffosuite.c

build/nice/solvecheck.bin: $(ENGINE) $(HEADERS) tests/solvecheck.c $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -D SOLVECHECK_POSITIONS=$(word 1,$(SOLVECHECK_NICE)) \
	  -D SOLVECHECK_EMPTIES=$(word 2,$(SOLVECHECK_NICE)) -o $@ $(ENGINE) tests/solvecheck.c

build/nice/ffosuite.bin: $(ENGINE) $(HEADERS) tests/ffosuite.c $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -o $@ $(ENGINE) tests/ffosuite.c

$(FFO1_NICE): $(FFO1)
	@mkdir -p $(@D)
	head -3 $< > $@

test: host-test nice-test

host-test: build/host/othello_text build/host/randgames build/host/solvecheck build/host/ffosuite build/host/ntcheck \
           build/host/train $(FFO1)
	@tools/runtest.sh host_text text tests/text.input host build/host/othello_text
	@tools/runtest.sh host_search search tests/search.input host build/host/othello_text
	@tools/runtest.sh host_nboard nboard tests/nboard.input host build/host/othello_text
	@tools/runtest.sh host_perft9 perft9 tests/perft9.input host build/host/othello_text
	@tools/runtest.sh host_randgames randgames$(RANDGAMES_HOST) /dev/null host build/host/randgames $(RANDGAMES_HOST)
	@tools/runtest.sh host_solvecheck solvecheck$(word 2,$(SOLVECHECK_HOST)) /dev/null host build/host/solvecheck $(SOLVECHECK_HOST)
	@tools/runtest.sh host_ffo1 ffo1 $(FFO1) host build/host/ffosuite
	@tools/runtest.sh host_ntcheck ntcheck$(NTCHECK_HOST) /dev/null host build/host/ntcheck $(NTCHECK_HOST)
	@tools/runtest.sh host_train train /dev/null host build/host/train --selftest build/test/train_selftest.bin

nice-test: build/nice/othello_text.bin build/nice/othello_screen.bin build/nice/randgames.bin \
           build/nice/solvecheck.bin build/nice/ffosuite.bin build/nice/ntcheck.bin \
           build/test/mon_othello_text.bin build/test/mon_othello_screen.bin $(FFO1_NICE) | $(NICEEMU)
	@tools/runtest.sh nice_text text tests/text.input nice build/nice/othello_text.bin
	@tools/runtest.sh nice_search search tests/search.input nice build/nice/othello_text.bin
	@tools/runtest.sh nice_nboard nboard tests/nboard.input nice build/nice/othello_text.bin
	@tools/runtest.sh nice_perft7 perft7 tests/perft7.input nice build/nice/othello_text.bin
	@tools/runtest.sh nice_randgames randgames$(RANDGAMES_NICE) /dev/null nice build/nice/randgames.bin
	@tools/runtest.sh nice_solvecheck solvecheck$(word 2,$(SOLVECHECK_NICE)) /dev/null nice build/nice/solvecheck.bin
	@tools/runtest.sh nice_ffo1 ffo1-3 $(FFO1_NICE) nice build/nice/ffosuite.bin
	@tools/runtest.sh nice_ntcheck ntcheck$(NTCHECK_NICE) /dev/null nice build/nice/ntcheck.bin
	@tools/runtest.sh nice_screen screen tests/screen.keys screen build/nice/othello_screen.bin
	@tools/runtest.sh nice_screen_game screen_game tests/screen_game.keys screen build/nice/othello_screen.bin
	@tools/runtest.sh mega65_text text tests/text.input nice build/test/mon_othello_text.bin
	@tools/runtest.sh mega65_search search tests/search.input nice build/test/mon_othello_text.bin
	@tools/runtest.sh mega65_perft7 perft7 tests/perft7.input nice build/test/mon_othello_text.bin
	@tools/runtest.sh mega65_screen screen tests/screen.keys screen build/test/mon_othello_screen.bin

perft-deep: build/host/othello_fast
	@tools/runtest.sh host_perft11 perft11 tests/perft11.input host build/host/othello_fast

# FFO positions 20 to 38, about a minute; 39, of 26 empty squares,
# takes more than a minute alone (LOG.md).
FFO20_38 = build/test/ffo20-38.obf
$(FFO20_38): $(FFO20)
	@mkdir -p $(@D)
	head -19 $< > $@

ffo-deep: build/host/ffosuite_fast $(FFO20_38)
	@tools/runtest.sh host_ffo20 ffo20-38 $(FFO20_38) host build/host/ffosuite_fast

# The trainer, which writes WEIGHTS every 100000 games, and prints the
# score of the learned evaluation against the hand-written one then.
train: build/host/train
	@mkdir -p $(dir $(WEIGHTS))
	build/host/train --games $(TRAIN_GAMES) --threads $(TRAIN_THREADS) $(TRAIN_ARGS) $(WEIGHTS)

# The weights of the builds, from a training, after a match-hand has
# shown them better than those before (doc/HISTORY.md says how they were
# made).
weights:
	cp $(WEIGHTS) $(NT_WEIGHTS_BIN)

match-hand: build/host/othello_fast
	$(PYTHON) tools/match.py --openings 50 --init1 "weights $(WEIGHTS)" --init2 "evaluation hand" \
	  build/host/othello_fast build/host/othello_fast

# Against random moves, at depth 4: it must win every game.
match-random: build/host/othello_fast
	$(PYTHON) tools/match.py --go1 "go depth 4" --min-score 20 build/host/othello_fast random

size: build/mega65/othello_text.bin build/mega65/othello_screen.bin build/test/mon_othello_text.bin \
      build/test/mon_othello_screen.bin | $(NICEEMU)
	$(PYTHON) tools/size.py --emu $(NICEEMU) --input tests/size.input --run build/test/mon_othello_text.bin \
	  --budget $(BUDGET) --check build/mega65/othello_text.bin
	$(PYTHON) tools/size.py --emu $(NICEEMU) --input tests/screen_game.keys --run build/test/mon_othello_screen.bin \
	  --budget $(BUDGET) --check build/mega65/othello_screen.bin

# The board. load.py loads a program under the monitor and types r, which
# runs it; what it prints then comes on the line, such as nc IP 4510 or
# picocom -b 2000000 DEVICE (NiceMonMEGA65's tools/README.md).
board-text: build/mega65/othello_text.bin
	$(PYTHON) $(NICEMON)/tools/load.py $(BOARDLINK) --monitor $<

board-screen: build/mega65/othello_screen.bin
	$(PYTHON) $(NICEMON)/tools/load.py $(BOARDLINK) --monitor $<

deps: $(NICE_CPU) $(NICEEMU) $(FFO1) $(FFO20)

$(FFO1):
	@mkdir -p $(@D)
	curl -sSfL -o $@.tmp $(EDAX_RAW)/fforum-1-19.obf
	echo "$(FFO1_SHA256)  $@.tmp" | sha256sum -c --quiet
	mv $@.tmp $@

$(FFO20):
	@mkdir -p $(@D)
	curl -sSfL -o $@.tmp $(EDAX_RAW)/fforum-20-39.obf
	echo "$(FFO20_SHA256)  $@.tmp" | sha256sum -c --quiet
	mv $@.tmp $@

deps/nice_cpu-$(NICE_CPU_TAG):
	git clone --quiet --depth 1 --branch $(NICE_CPU_TAG) -c advice.detachedHead=false \
	  https://github.com/MJoergen/nice_cpu $@

deps/niceemu-$(NICE_CPU_TAG)-x86_64-linux/niceemu:
	@mkdir -p deps
	curl -sSfL -o deps/$(NICEEMU_TGZ) $(RELEASE_URL)/$(NICEEMU_TGZ)
	echo "$(NICEEMU_SHA256)  deps/$(NICEEMU_TGZ)" | sha256sum -c --quiet
	tar xzf deps/$(NICEEMU_TGZ) -C deps
	rm deps/$(NICEEMU_TGZ)
	@touch $@

# The levels against Edax at its levels of the same number (LOG.md has
# all the pairs measured), on the host: the levels are depths, so the
# moves, and the strength, are those of the MEGA65.
RATE_PAIRS    ?= 1:1 2:2 3:3 4:4 5:5
RATE_OPENINGS ?= 50
rate: build/host/othello_fast $(EDAX)
	tools/rate.sh build/host/othello_fast "$(EDAX_GTP)" $(RATE_OPENINGS) $(RATE_PAIRS)

# Edax: its source at EDAX_COMMIT, built, and its evaluation weights.
# Not in make deps: only make rate needs it.
edax: $(EDAX)

$(EDAX):
	rm -rf deps/edax.tmp
	git init -q deps/edax.tmp
	cd deps/edax.tmp && git fetch -q --depth 1 $(EDAX_GIT) $(EDAX_COMMIT) && git checkout -q FETCH_HEAD
	$(MAKE) -C deps/edax.tmp/src build ARCH=$(EDAX_ARCH) OS=linux
	curl -sSfL -o deps/edax.tmp/eval.7z $(EDAX_EVAL)
	echo "$(EDAX_EVAL_SHA256)  deps/edax.tmp/eval.7z" | sha256sum -c --quiet
	cd deps/edax.tmp && 7z x -y eval.7z > /dev/null && rm eval.7z
	rm -rf deps/edax && mv deps/edax.tmp deps/edax

clean:
	rm -rf build

distclean: clean
	rm -rf deps
