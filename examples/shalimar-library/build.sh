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

if [ -n "${CC:-}" ]; then
    compiler="$CC"
    "$CC" -c -I "$RT" "$here/stats.c" -o "$here/stats.o"
else
    [ -x "$CC1" ] || { echo "no $CC1 - build cc1 first, or set CC=cc"; exit 2; }
    compiler="cc1"
    "$CC1" -c -I "$RT" "$here/stats.c" -o "$here/stats.o"
fi

ar rcs "$here/libstats.a" "$here/stats.o"

echo "$compiler compiled stats.c; ar made libstats.a"
echo
echo "Now build a Shalimar program against it:"
echo "  shc prog.shm --with=$here/libstats.a -o prog"
echo
echo "There is one ready to run in Compiler-S/examples/using-a-library."
