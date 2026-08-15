#!/bin/sh
# What C90 says, and what this compiler does about it.
#
# Not a test suite, and the difference matters. tests/run.sh holds cases the
# compiler must pass and fails the build when one breaks; every case in it was
# written because the feature already worked. That makes it a good ratchet and
# a poor explorer - it can only ever report on ground already taken.
#
# This asks the other question. Each file in tests/c90/ is one C90 feature that
# cc1 did not accept when the corpus was written, so a fresh run reads as all
# REFUSES and that is correct rather than alarming - the features that work are
# in tests/cases, where 396 of them are checked against gcc on every build.
# What this is, is the gap list with its own proof attached: an entry flips to
# "accepts" the day it is implemented, and nothing has to be remembered.
#
# gcc -std=c90 -pedantic is consulted on the same file, so a refusal separates
# into "cc1 is behind the standard" and "the case is wrong" without argument.
#
# It exits 0 whatever it finds. A feature that is missing is a fact about
# today, not a failure - the point is to be able to re-derive the list in
# docs/STATUS.md rather than to trust that it is still true. That list was two
# entries long and wrong until this was run for the first time, which is the
# whole argument for having it.
#
#   ./tests/c90-probe.sh
#
# Needs gcc for the second opinion; without it the verdict column reads "?" and
# the rest still works.

set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1"
SRC="$ROOT/tests/c90"
OUT="$ROOT/tests/out-c90"

[ -x "$CC1" ] || { echo "c90-probe: no cc1 - run make first"; exit 2; }

rm -rf "$OUT" && mkdir -p "$OUT"
have_gcc=0
command -v gcc >/dev/null 2>&1 && have_gcc=1

accepted=0
refused=0

printf '%-22s %-10s %s\n' "FEATURE" "cc1" "gcc -std=c90 -pedantic"
printf '%-22s %-10s %s\n' "----------------------" "----------" "----------------------"

for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)

    if "$CC1" -S "$src" -o "$OUT/$name.s" > "$OUT/$name.err" 2>&1; then
        mine="accepts"
        accepted=$((accepted + 1))
    else
        mine="REFUSES"
        refused=$((refused + 1))
    fi

    theirs="?"
    if [ "$have_gcc" -eq 1 ]; then
        if gcc -std=c90 -pedantic -w -c "$src" -o /dev/null 2>/dev/null; then
            theirs="valid C90"
        else
            theirs="not valid C90"
        fi
    fi

    printf '%-22s %-10s %s\n' "$name" "$mine" "$theirs"
done

echo
echo "cc1 accepts $accepted of $((accepted + refused))."
echo
echo "Anything reading 'REFUSES' against 'valid C90' is a gap, and belongs in"
echo "the Not implemented section of docs/STATUS.md. Two of them are there"
echo "as declined rather than pending - K&R definitions and trigraphs, both"
echo "removed by C23 - and adding those would mean implementing what the"
echo "language has since deleted. Everything else is simply missing."
exit 0
