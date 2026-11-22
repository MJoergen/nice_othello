#!/bin/sh
# runtest.sh NAME CHECK INPUT host|nice|screen|board|board-screen PROGRAM [ARGS...]
#
# Runs PROGRAM with INPUT as its console input, on the host, in
# niceemu ($NICEEMU, with --step-time, so that its clock counts
# steps), or on the board under its monitor (board, through
# tools/board.py, with the link in $BOARDLINK, such as
# "--eth 192.168.1.65"), and leaves its output in build/test/NAME.raw.
# screen is nice with --color, so that the screen at the halt, with its
# colors, is in the output too, as lines that start with "|", and
# board-screen is board with it. On the board, the lines before the
# banner of the text front end ("nice_othello "), which the monitor
# prints, and its "Returned N" are left out.
#
# The lines that match a pattern of tests/CHECK.grep (with the digits
# at the end of CHECK left out: perft7 uses perft.grep), after the
# prompts "> " and the times (" time N", and "/T" at the end of NBoard's
# "=== M/EVAL/T") are taken off, and niceemu's
# report after the halt (but for the screen), and then
# "exit N" with main's result, with the version that follows
# "nice_othello " ($VERSION, which the Makefile exports) as VERSION,
# must be tests/CHECK.expected. Prints PASS or FAIL, and exits with 1
# on a failure.
set -u
name=$1 check=$2 input=$3 kind=$4 prog=$5
shift 5
dir=build/test
mkdir -p $dir
raw=$dir/$name.raw
out=$dir/$name.out

if [ "$kind" = board ] || [ "$kind" = board-screen ]; then
    color=
    [ "$kind" = board-screen ] && color=--color
    python3 tools/board.py ${BOARDLINK:?the Makefile sets BOARDLINK} $color -i "$input" "$prog" \
        < /dev/null > $raw.all
    r=$(grep -o '^Returned [0-9a-f]*' $raw.all | cut -c10-)
    if [ -n "$r" ]; then
        status=$(( 0x$r ))
    else
        status="none (the program did not return to the monitor)"
    fi
    if [ "$kind" = board ]; then
        sed -n '/^\(> \)*nice_othello /,$p' $raw.all
    else
        cat $raw.all
    fi | grep -v '^Returned ' > $raw
    rm $raw.all
elif [ "$kind" = nice ] || [ "$kind" = screen ]; then
    color=
    [ "$kind" = screen ] && color=--color
    "$NICEEMU" --step-time $color -i "$input" "$prog" < /dev/null > $raw
    r1=$(grep -o 'R1  [0-9a-f]*' $raw | cut -c5-)
    if grep -q 'Halted (HALT)' $raw && [ -n "$r1" ]; then
        status=$(( 0x$r1 ))
    else
        status="none (the program did not halt)"
    fi
else
    "$prog" "$@" < "$input" > $raw
    status=$?
fi

expected=tests/$check.expected
version="s/nice_othello ${VERSION:?the Makefile exports VERSION}/nice_othello VERSION/g"
{ sed '/^Halted (/,/^Screen/d; s/^\(> \)*//; s/ time [0-9][0-9]*//; s/^\(=== [^/]*\/[^/]*\)\/[0-9.]*$/\1/' $raw | grep -f tests/${check%%[0-9]*}.grep | sed "$version"; echo "exit $status"; } > $out
if cmp -s $out $expected; then
    echo "PASS $name"
else
    echo "FAIL $name"
    diff $expected $out
    exit 1
fi
