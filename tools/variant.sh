#!/bin/sh
# variant.sh NAME MASK STAGES [GAMES [OPENINGS]]
#
# Measures a choice of the N-tuple evaluation: the patterns MASK (NT_MASK,
# a bit for each of nt_defs in src/engine/ntuple.c) in STAGES stages.
# Builds the trainer and the fast host engine with them into
# build/variants/NAME/, trains GAMES games (3000000) on $THREADS threads
# (8), and plays the weights against the evaluation written by hand at
# depth 4, from OPENINGS openings (200), each with both colours. $SEED
# (1) seeds the training. Prints a line of the results, which it
# appends to build/variants/results.txt.
set -eu
name=$1 mask=$2 stages=$3 games=${4:-3000000} openings=${5:-200}
threads=${THREADS:-8}
dir=build/variants/$name
mkdir -p "$dir"
flags="-O2 -DNDEBUG -D NT_MASK=$mask -D NT_STAGES=$stages"
echo 'const char nice_othello_version[] = "variant";' > "$dir/version.c"
cc $flags -DNT_TLS=_Thread_local -pthread -o "$dir/train" src/engine/*.c tools/train.c -lm
cc $flags -DTT_BITS=20 -o "$dir/othello" src/engine/*.c "$dir/version.c" src/ui/text.c src/platform/host.c

"$dir/train" --games "$games" --threads "$threads" --report "$games" --seed "${SEED:-1}" "$dir/weights.bin" > "$dir/train.txt"
head=$(head -1 "$dir/train.txt")
places=$(echo "$head" | sed 's/.* places \([0-9]*\).*/\1/')
weights=$(echo "$head" | sed 's/.* weights \([0-9]*\).*/\1/')
ply1=$(tail -1 "$dir/train.txt" | sed 's/.* score \([0-9.]*%\).*/\1/')

python3 tools/match.py --openings "$openings" --init1 "weights $dir/weights.bin" \
    --go1 "go depth 4" --go2 "go depth 4" "$dir/othello" "$dir/othello" > "$dir/match.txt"
score=$(tail -1 "$dir/match.txt")

line="$name mask $mask stages $stages places $places weights $weights games $games 1-ply $ply1 depth-4 $score"
echo "$line"
echo "$line" >> build/variants/results.txt
