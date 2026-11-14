# nice_othello: an Othello engine for the NICE system.
#
# The NICE toolchain comes from nice_cpu at a pinned release tag.
# make deps fetches it, and niceemu, into deps/. NICE_CPU=DIR and
# NICEEMU=PATH use local copies of nice_cpu and niceemu instead.
# make help lists the targets.

NICE_CPU_TAG    := V2.0
NICEEMU_SHA256  := 7726cfdf6c86513d156ed75e52c287a7d88ef0c82c7bb49fa02558dde0cf7cf1

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

# The MEGA65 gives a program under the monitor the words 0x8000 to
# 0x1EFFF, 92K, for its image, its linear memory, and the NICE stack
# (NiceMonMEGA65 V1.2.1, as in nice_chess).
BUDGET = 94176

ENGINE  = $(wildcard src/engine/*.c)
# The headers, and the Makefile, whose flags every build takes, so that
# a change of the flags rebuilds.
HEADERS = $(wildcard src/engine/*.h) src/platform/platform.h src/ui/version.h Makefile
# The version, for the front end (VERSION).
VERSIONC = build/gen/version.c

# NICE: optimized for speed, without asserts, and without the bounds
# checks of wasm's memory accesses, which the host builds' asserts and
# CHECK_STATE stand in for. --trust-alignment leaves out the alignment
# test of each access of 4 bytes, and --byte-windows reaches the memory
# through the byte windows of nice_cpu's SYSTEM.md §9 (niceemu and
# NiceMonMEGA65 from V2.0 on), as in nice_chess.
NICEFLAGS = -O 2 -D NDEBUG --no-bounds-checks --trust-alignment --byte-windows \
            --stack 8192 --memory 65536 --symbols --keep
# The MEGA65: the same, built for the monitor, which loads it at 0x8000.
MONFLAGS = $(NICEFLAGS) --monitor

# The host: with asserts, and a check of the moves and the flips of
# every node of perft against the plain generator of check.c.
HOSTFLAGS = -O2 -W -Wall -Wextra -DCHECK_STATE
# The host, fast: without asserts and the check.
FASTFLAGS = -O2 -W -Wall -Wextra -DNDEBUG

# The random games of tests/randgames.c, on the host and in niceemu.
RANDGAMES_HOST = 10000
RANDGAMES_NICE = 200

.PHONY: FORCE help all test host-test nice-test perft-deep size deps clean distclean board-text
help:
	@echo "nice_othello: an Othello engine for the NICE system"
	@echo ""
	@echo "  make help          show this text (the default)"
	@echo "  make deps          fetch nice_cpu $(NICE_CPU_TAG) and niceemu $(NICE_CPU_TAG) into deps/"
	@echo "  make all           build the text front end, for niceemu, for the MEGA65, and for the host"
	@echo "  make test          the tests, on the host and in niceemu"
	@echo "  make perft-deep    perft to depth 11 on the host, without the checks (a few seconds)"
	@echo "  make size          the size of the MEGA65 build, against the board's budget"
	@echo "  make clean         remove build/"
	@echo "  make distclean     remove build/ and deps/"
	@echo ""
	@echo "On the MEGA65, under NiceMonMEGA65's monitor, with ETH=IP (the board's"
	@echo "Ethernet port) or PORT=/dev/ttyUSBn (its debug UART), and the tools of a"
	@echo "checkout of NiceMonMEGA65 (NICEMON=DIR, ../NiceMonMEGA65 by default):"
	@echo ""
	@echo "  make board-text    load and run the text front end, then talk to it over the line"
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

$(VERSIONC): FORCE
	@mkdir -p $(@D)
	@t='const char nice_othello_version[] = "$(VERSION)";'; \
	  [ "$$t" = "$$(cat $@ 2>/dev/null)" ] || echo "$$t" > $@

FORCE:

all: build/nice/othello_text.bin build/mega65/othello_text.bin build/host/othello_text build/host/othello_fast

build/nice/othello_text.bin: $(ENGINE) $(HEADERS) src/ui/text.c src/platform/nice.c $(VERSIONC) $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(NICEFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/text.c src/platform/nice.c

build/mega65/othello_text.bin: $(ENGINE) $(HEADERS) src/ui/text.c src/platform/nice.c $(VERSIONC) $(TOOLCHAIN)
	@mkdir -p $(@D)
	$(NICEBUILD) $(MONFLAGS) -o $@ $(ENGINE) $(VERSIONC) src/ui/text.c src/platform/nice.c

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

test: host-test nice-test

host-test: build/host/othello_text build/host/randgames
	@tools/runtest.sh host_text text tests/text.input host build/host/othello_text
	@tools/runtest.sh host_perft9 perft9 tests/perft9.input host build/host/othello_text
	@tools/runtest.sh host_randgames randgames$(RANDGAMES_HOST) /dev/null host build/host/randgames $(RANDGAMES_HOST)

nice-test: build/nice/othello_text.bin build/nice/randgames.bin build/test/mon_othello_text.bin | $(NICEEMU)
	@tools/runtest.sh nice_text text tests/text.input nice build/nice/othello_text.bin
	@tools/runtest.sh nice_perft7 perft7 tests/perft7.input nice build/nice/othello_text.bin
	@tools/runtest.sh nice_randgames randgames$(RANDGAMES_NICE) /dev/null nice build/nice/randgames.bin
	@tools/runtest.sh mega65_text text tests/text.input nice build/test/mon_othello_text.bin
	@tools/runtest.sh mega65_perft7 perft7 tests/perft7.input nice build/test/mon_othello_text.bin

perft-deep: build/host/othello_fast
	@tools/runtest.sh host_perft11 perft11 tests/perft11.input host build/host/othello_fast

size: build/mega65/othello_text.bin build/test/mon_othello_text.bin | $(NICEEMU)
	$(PYTHON) tools/size.py --emu $(NICEEMU) --input tests/size.input --run build/test/mon_othello_text.bin \
	  --budget $(BUDGET) --check build/mega65/othello_text.bin

# The board. load.py loads a program under the monitor and types r, which
# runs it; what it prints then comes on the line, such as nc IP 4510 or
# picocom -b 2000000 DEVICE (NiceMonMEGA65's tools/README.md).
board-text: build/mega65/othello_text.bin
	$(PYTHON) $(NICEMON)/tools/load.py $(BOARDLINK) --monitor $<

deps: $(NICE_CPU) $(NICEEMU)

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

clean:
	rm -rf build

distclean: clean
	rm -rf deps
