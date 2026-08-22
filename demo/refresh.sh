#!/usr/bin/env bash
#
# Regenerate the recorded worked examples, and say so if what is checked in no
# longer matches what the compiler emits.
#
# A recorded artifact that quietly goes stale is worse than no artifact: it
# reads as current and is not. This has already happened once - the return
# label became .L.return.main when functions arrived, and gcd.s still said
# .L.return. So a mismatch exits non-zero, ready to be wired into a hook.
#
# Each example is also assembled and run, and must still give the answer and
# the output recorded here. An artifact worth keeping is one that still works.
#
#   ./demo/refresh.sh          check, and rewrite anything that has changed
#   ./demo/refresh.sh --check  check only, change nothing

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CHECK_ONLY="${1:-}"

[ -x "$ROOT/cc1.exe" ] || { echo "FATAL: cc1 not built - run ./build first"; exit 1; }

# case name : expected exit status : expected stdout (\n for newlines)
EXAMPLES=(
    "gcd:6:"
    "out_factorial:0:120\n"
)

status=0

for entry in "${EXAMPLES[@]}"; do
    name="${entry%%:*}"
    rest="${entry#*:}"
    wantExit="${rest%%:*}"
    wantOut="${rest#*:}"

    case="$ROOT/tests/cases/$name.c"
    recorded="$ROOT/demo/$name.s"

    fresh="$(mktemp)"
    "$ROOT/cc1.exe" "$case" -o "$fresh" || { echo "FATAL: cc1 could not compile $case"; rm -f "$fresh"; exit 1; }

    # -x assembler because the temp file has no .s suffix, and gcc picks its
    # language from the extension - without it the assembly is read as a linker
    # script and fails with "file format not recognized".
    gcc -x assembler "$fresh" -o "$fresh.bin" || {
        echo "FATAL: $name - the emitted assembly will not assemble"; rm -f "$fresh" "$fresh.bin"; exit 1; }

    got="$(timeout 5 "$fresh.bin")"; gotExit=$?
    rm -f "$fresh.bin"

    if [ "$gotExit" != "$wantExit" ]; then
        echo "FATAL: $name exited $gotExit, expected $wantExit"
        rm -f "$fresh"; exit 1
    fi
    if [ "$got" != "$(printf "$wantOut")" ]; then
        echo "FATAL: $name printed '$got', expected '$(printf "$wantOut")'"
        rm -f "$fresh"; exit 1
    fi

    if [ -f "$recorded" ] && diff -q "$recorded" "$fresh" >/dev/null; then
        echo "demo/$name.s is current ($(wc -l < "$fresh") lines, exit $gotExit)"
        rm -f "$fresh"
        continue
    fi

    if [ "$CHECK_ONLY" = "--check" ]; then
        echo "demo/$name.s is STALE - the compiler now emits something different:"
        diff -u "$recorded" "$fresh" | head -20
        status=1
        rm -f "$fresh"
        continue
    fi

    cp "$fresh" "$recorded"
    echo "demo/$name.s rewritten ($(wc -l < "$recorded") lines, exit $gotExit)"
    rm -f "$fresh"
done

exit $status
