#!/usr/bin/env bash
#
# Did any byte of the assembly move?
#
# Every other suite here asks whether the compiler is *right*. This one asks a
# narrower question that the others cannot: whether it produces exactly what it
# produced before. That is the question a refactor has to answer, and answering
# it by running the differential suites is not enough - they compare cc1
# against gcc on what a program prints, so a change that reorders two
# independent instructions, or renames a label, or drops a directive the
# assembler did not need, passes all of them while the output is not the same
# text at all.
#
# It exists because it earned itself twice in one day. Cutting the emission
# seam into the x86-64 generator touched 417 sites; the corpus stayed green
# throughout, and this is what proved the 1,648 files were byte-identical
# rather than merely still correct. Then, converting the MASM spelling to
# append to a string, 'o_ += frameSize' compiled cleanly and appended a
# *character* - an int converted to char - to 313 files. Every suite still
# passed. This named the 313.
#
#   ./tests/fingerprint.sh            compare against tests/fingerprint.txt
#   ./tests/fingerprint.sh --record   write tests/fingerprint.txt
#
# Run it before a refactor and after. A difference is not automatically a
# fault: when the output is *meant* to move, --record and let the diff of
# fingerprint.txt be reviewed like any other change. That is the point of
# keeping it in the repository rather than in a scratch directory - the file
# is a record of what the compiler emitted, and git already knows how to show
# what changed about it.
#
# **No assembler and no linker.** This is 'cc1 -S' and nothing else, so it runs
# wherever cc1 builds - the Mac, the Linux box, a Windows host - and it covers
# all four spellings from any one of them, including targets that machine
# cannot execute. The fingerprint is the same on every host, which is checked:
# nothing in the output depends on the machine it was produced on, or on the
# path the compiler was invoked with.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC1="$ROOT/cc1"
FILE="$ROOT/tests/fingerprint.txt"
WORK="$ROOT/tests/out-fingerprint"

# Everything below names its source as 'tests/cases/x.c' from the repository
# root, and never by absolute path. That is not tidiness: <assert.h> expands
# __FILE__ into the assembly, so the path the compiler was *given* is part of
# the output. Handing it an absolute path makes the fingerprint a record of
# where the repository happens to sit, and the same commit then fails on
# another machine for no reason but its own directory. This was found by
# running the Mac's fingerprint on the Linux box, which is the only way it
# could have been.
cd "$ROOT" || exit 1

[ -x "$CC1" ] || { echo "FATAL: $CC1 not built - run ./build first"; exit 1; }

# 'shasum -a 256' on the Mac, 'sha256sum' on Linux. Same digest either way.
if command -v sha256sum >/dev/null 2>&1; then HASH="sha256sum"
elif command -v shasum >/dev/null 2>&1;    then HASH="shasum -a 256"
else echo "FATAL: no sha256 tool"; exit 1
fi

# Every spelling the compiler can write, named as it will appear in the
# fingerprint. The Windows target twice, because MASM and the GNU form of the
# same target are different text and a change can move one without the other.
SPELLINGS="x86_64-linux:
x86_64-windows:
x86_64-windows:-masm=gnu
arm64-darwin:"

rm -rf "$WORK"
mkdir -p "$WORK"

generate() {
    for spelling in $SPELLINGS; do
        arch="${spelling%%:*}"
        flag="${spelling#*:}"
        tag="$arch"
        [ -n "$flag" ] && tag="$arch$flag"
        for src in tests/cases/*.c; do
            case_name="$(basename "$src" .c)"
            out="$WORK/one.s"
            # A refusal is recorded rather than skipped. What the compiler
            # declines to compile is as much a fact about it as what it emits,
            # and a refactor that silently starts accepting - or refusing -
            # a case should show up here as loudly as changed instructions.
            if "$CC1" -arch "$arch" $flag -S "$src" -o "$out" >/dev/null 2>&1; then
                digest="$($HASH "$out" | cut -d' ' -f1)"
            else
                digest="REFUSED"
            fi
            printf '%s  %s.%s\n' "$digest" "$case_name" "$tag"
        done
    done
}

# Sorted by name, not by digest. A digest sort scatters the whole file when
# one case changes, and this is meant to be read as a diff: one case that
# moved should be one line that moved.
generate | LC_ALL=C sort -k2 > "$WORK/now.txt"
count=$(wc -l < "$WORK/now.txt" | tr -d ' ')

if [ "${1:-}" = "--record" ]; then
    mv "$WORK/now.txt" "$FILE"
    rm -rf "$WORK"
    echo "recorded $count fingerprints in tests/fingerprint.txt"
    exit 0
fi

if [ ! -f "$FILE" ]; then
    echo "no tests/fingerprint.txt - run './tests/fingerprint.sh --record' first"
    rm -rf "$WORK"
    exit 1
fi

if cmp -s "$FILE" "$WORK/now.txt"; then
    echo "fingerprint  $count files, all identical"
    rm -rf "$WORK"
    exit 0
fi

# Named, not counted. A bare "17 files differ" is what this suite exists to
# refuse: it is the difference between knowing something moved and knowing
# what moved, and the second is the only one that leads anywhere.
echo "fingerprint  $count files"
echo ""
echo "--- these are not what they were ---"
LC_ALL=C awk 'NR==FNR { was[$2]=$1; next }
     { if (!($2 in was))      printf "  %-44s new\n", $2
       else if (was[$2] != $1) printf "  %-44s %s -> %s\n", $2, was[$2], $1
       delete was[$2] }
     END { for (k in was) printf "  %-44s gone\n", k }' \
    "$FILE" "$WORK/now.txt" | LC_ALL=C sort

echo ""
echo "If the output was meant to move, './tests/fingerprint.sh --record' and"
echo "review the diff of tests/fingerprint.txt."
rm -rf "$WORK"
exit 1
