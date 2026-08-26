#!/bin/sh
# Build libstats, a library a Shalimar program can call.
#
#   ./build.sh                 with cc1, which is what this repository is for
#   CC=cc ./build.sh           with the host's compiler instead
#
# Two steps, and the split matters: **a compiler makes an object, and `ar`
# makes a library.** cc1 owns the first and has nothing to do with the second.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
CC1="${CC1:-$here/../../cc1.exe}"
RT="${RT:-$here/../../../Compiler-S/runtime}"

[ -f "$RT/shmrt.h" ] || { echo "no $RT/shmrt.h - set RT to Compiler-S/runtime"; exit 2; }

# Three files, three kinds of argument: stats.c is reals and real arrays,
# tally.c is integers and integer arrays, text.c is char arrays. They go into
# one library because a library is a bag of objects and a program takes what
# it calls.
UNITS="stats tally text"

if [ -n "${CC:-}" ]; then
    compiler="$CC"
    for u in $UNITS; do "$CC" -c -I "$RT" "$here/$u.c" -o "$here/$u.o"; done
else
    [ -x "$CC1" ] || { echo "no $CC1 - build cc1 first, or set CC=cc"; exit 2; }
    compiler="cc1"
    for u in $UNITS; do "$CC1" -c -I "$RT" "$here/$u.c" -o "$here/$u.o"; done
fi

objects=""
for u in $UNITS; do objects="$objects $here/$u.o"; done
ar rcs "$here/libstats.a" $objects

echo "$compiler compiled$(for u in $UNITS; do printf ' %s.c' "$u"; done); ar made libstats.a"
echo
echo "Now build a Shalimar program against it:"
echo "  shc prog.shm --with=$here/libstats.a -o prog"
echo
echo "There is one ready to run in Compiler-S/examples/using-a-library."
