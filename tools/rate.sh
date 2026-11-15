#!/bin/sh
# rate.sh PROGRAM EDAX OPENINGS PAIRS...
#
# Rates the levels of the text front end PROGRAM against Edax: for each
# pair OURS:EDAX, such as 3:3, plays tools/match.py from OPENINGS
# openings, each with both colours, with "level OURS" against EDAX, a
# GTP player of match.py, at its level EDAX (-l). Prints a line for
# each pair: the score, and the difference of Elo that it means, ours
# minus Edax's. The games go to build/rate/OURS-EDAX.txt.
set -eu
prog=$1 edax=$2 openings=$3
shift 3
mkdir -p build/rate
for pair in "$@"; do
    ours=${pair%%:*} theirs=${pair#*:}
    log=build/rate/$ours-$theirs.txt
    python3 tools/match.py --openings "$openings" --init1 "level $ours" --go1 go \
        "$prog" "$edax -l $theirs" > "$log"
    tail -1 "$log" | awk -v o="$ours" -v t="$theirs" '{
        sub(",", "", $2); split($2, s, "/"); p = s[1] / s[2]
        q = p < 0.005 ? 0.005 : p > 0.995 ? 0.995 : p
        printf "level %d against Edax level %d: %s/%s, %.1f%%, Elo %+d\n",
               o, t, s[1], s[2], 100 * p, -400 * log(1 / q - 1) / log(10)
    }'
done
