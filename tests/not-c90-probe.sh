#!/bin/sh
# What C90 forbids, and what this compiler does about it.
#
# The other half of tests/c90-probe.sh, and it exists because that one cannot
# see this. Every file in tests/c90/ is valid C90 that cc1 refuses, so the probe
# built on it can only ever find features that are missing. A feature that is
# *extra* - something C90 forbids and cc1 takes anyway - leaves no trace there,
# and none in tests/run.sh either, where a case is written only once it passes.
# Both suites are silent about the same thing, which is how five extensions came
# to be in this compiler with two of them written down.
#
# So: each file here is a program C90 does not allow. "accepts" means cc1 takes
# it, which is a divergence from the language it claims and belongs in
# docs/STATUS.md by name. "refuses" means the check exists. An entry flips to
# "refuses" the day one is added, the same way an entry in the other corpus
# flips to "accepts" the day a feature lands.
#
# An extension is not automatically a fault. Variadic macros are here and are
# kept deliberately, because the alternative in real code is no macro at all.
# What is a fault is an extension nobody wrote down - a program that compiles
# here and not with the compiler this one is measured against, for a reason
# neither the docs nor the suite can name.
#
# It exits 0 whatever it finds, like its counterpart.
#
#   ./tests/not-c90-probe.sh
#
# Needs gcc for the second opinion; without it the verdict column reads "?".

set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1.exe"
SRC="$ROOT/tests/not-c90"
OUT="$ROOT/tests/out-not-c90"

[ -x "$CC1" ] || { echo "not-c90-probe: no cc1 - run make first"; exit 2; }

rm -rf "$OUT" && mkdir -p "$OUT"
have_gcc=0
command -v gcc >/dev/null 2>&1 && have_gcc=1

accepted=0
refused=0

printf '%-24s %-10s %s\n' "WHAT C90 FORBIDS" "cc1" "gcc -std=c90 -pedantic-errors"
printf '%-24s %-10s %s\n' "------------------------" "----------" "-----------------------------"

for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)

    if "$CC1" -S "$src" -o "$OUT/$name.s" > "$OUT/$name.err" 2>&1; then
        mine="accepts"
        accepted=$((accepted + 1))
    else
        mine="refuses"
        refused=$((refused + 1))
    fi

    theirs="?"
    if [ "$have_gcc" -eq 1 ]; then
        if gcc -std=c90 -pedantic-errors -c "$src" -o /dev/null 2>/dev/null; then
            theirs="ACCEPTS - case is wrong"
        else
            theirs="not valid C90"
        fi
    fi

    printf '%-24s %-10s %s\n' "$name" "$mine" "$theirs"
done

echo
echo "cc1 accepts $accepted of $((accepted + refused))."
echo
echo "Every 'accepts' is an extension to the language this compiler claims, and"
echo "each one must be named in docs/STATUS.md. Two are kept on purpose -"
echo "variadic macros and GNU's ', ## __VA_ARGS__' - because the alternative in"
echo "real code is no macro at all. The rest are accidents of the lexer and the"
echo "parser being more permissive than the standard, and cost nothing to keep"
echo "until a program relies on one and then moves to another compiler."
echo
echo "A line reading 'ACCEPTS - case is wrong' means gcc took it too, so the"
echo "file is not the C90 violation it claims to be and should be fixed here."
exit 0
