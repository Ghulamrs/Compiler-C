#!/usr/bin/env bash
#
# Regenerate the recorded worked example, and say so if what is checked in no
# longer matches what the compiler emits.
#
# A recorded artifact that quietly goes stale is worse than no artifact: it
# reads as current and is not. So this exits non-zero on a mismatch, which
# means it can be wired into a hook or CI later without changing anything.
#
#   ./demo/refresh.sh          check, and rewrite gcd.s if it has changed
#   ./demo/refresh.sh --check  check only, change nothing

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CASE="$ROOT/tests/cases/gcd.c"
RECORDED="$ROOT/demo/gcd.s"
CHECK_ONLY="${1:-}"

[ -x "$ROOT/cc1" ] || { echo "FATAL: cc1 not built - run ./build first"; exit 1; }

fresh="$(mktemp)"
trap 'rm -f "$fresh" "$fresh.bin"' EXIT

"$ROOT/cc1" "$CASE" -o "$fresh" || { echo "FATAL: cc1 could not compile $CASE"; exit 1; }

# The artifact is only worth recording if it still runs and still answers 6.
# -x assembler because the temp file has no .s suffix, and gcc picks its
# language from the extension - without it the assembly is read as a linker
# script and fails with "file format not recognized".
gcc -x assembler "$fresh" -o "$fresh.bin" || { echo "FATAL: the emitted assembly will not assemble"; exit 1; }
timeout 5 "$fresh.bin"; got=$?
if [ "$got" != 6 ]; then
    echo "FATAL: gcd(48,18) came out as $got, not 6"
    exit 1
fi

if [ -f "$RECORDED" ] && diff -q "$RECORDED" "$fresh" >/dev/null; then
    echo "demo/gcd.s is current (gcd(48,18) = $got)"
    exit 0
fi

if [ "$CHECK_ONLY" = "--check" ]; then
    echo "demo/gcd.s is STALE - the compiler now emits something different:"
    diff -u "$RECORDED" "$fresh" | head -40
    exit 1
fi

cp "$fresh" "$RECORDED"
echo "demo/gcd.s rewritten (gcd(48,18) = $got)"
