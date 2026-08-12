#!/usr/bin/env bash
#
# Differential regression suite.
#
# Every case is compiled twice - once by cc1, once by gcc - both binaries are
# run, and a case passes only when the two agree on the exit status AND on what
# they printed, and both match the expected exit code on the first line.
#
# Comparing against gcc rather than against expectations alone is the point.
# An expectation is my opinion about C; gcc is the reference implementation
# sitting on the same disk. Where the two disagree the case is wrong until
# proven otherwise. That has already caught four wrong expectations of mine -
# each reported as "both gave X", which is the suite saying the compilers agree
# and the arithmetic in the case does not.
#
# Every binary runs under a timeout. Once a language has loops, a codegen bug
# stops giving wrong answers and starts giving none: breaking the remainder
# fixup turned collatz.c into an infinite loop and hung the suite until it was
# killed by hand.
#
# Cases run in parallel, because they are independent and because the work is
# not this compiler. Measured over 191 cases: cc1 accounts for 0.3s of about
# 12s, and the rest is gcc assembling, gcc building the reference, and running
# two binaries per case. Output is collected per case and printed in name order
# afterwards, so a parallel run reads exactly like a serial one - a test suite
# whose output shuffles between runs is a test suite nobody trusts.
#
#   ./tests/run.sh              all cases
#   ./tests/run.sh gcd          cases whose name contains "gcd"
#   ./tests/run.sh '' 1         serially, which is what to use when debugging
#
# Exits non-zero if any case fails.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC1="$ROOT/cc1"
OUT="$ROOT/tests/out"
LIMIT=5

# --- one case, run in its own process -------------------------------------
# Invoked by the parallel loop below as "run.sh --one <file>". Prints nothing
# on success and the failure text otherwise; the exit status carries the
# verdict. Kept as a mode of this script rather than a second file so the two
# can never drift apart.
if [ "${1:-}" = "--one" ]; then
    case_file="$2"
    name="$(basename "$case_file" .c)"

    expected="$(sed -n '1s|^// expect: *\([0-9-]*\).*|\1|p' "$case_file")"
    if [ -z "$expected" ]; then
        echo "FAIL $name - no '// expect: N' on line 1"; exit 1
    fi

    if ! "$CC1" "$case_file" -o "$OUT/$name.s" 2> "$OUT/$name.err"; then
        echo "FAIL $name - cc1 rejected it:"; sed 's/^/       /' "$OUT/$name.err"; exit 1
    fi
    if ! gcc "$OUT/$name.s" -o "$OUT/$name.ours" 2>> "$OUT/$name.err"; then
        echo "FAIL $name - our assembly would not assemble:"
        sed 's/^/       /' "$OUT/$name.err"; exit 1
    fi
    if ! gcc -w "$case_file" -o "$OUT/$name.gcc" 2>> "$OUT/$name.err"; then
        echo "FAIL $name - gcc rejected the case itself (the case is wrong)"; exit 1
    fi

    # Captured through command substitution rather than redirected to a file,
    # which is what keeps the shell's own "Segmentation fault" notice out of the
    # report: that notice is printed by whichever shell waits for the program,
    # so redirecting the program's stderr - or a subshell's - does not silence
    # it. A crash is already legible as an exit status of 139.
    #
    # Command substitution drops trailing newlines. Both sides are captured the
    # same way, so the comparison stays like for like.
    ourOut="$( timeout "$LIMIT" "$OUT/$name.ours" 2>/dev/null )"; ours=$?
    gccOut="$( timeout "$LIMIT" "$OUT/$name.gcc"  2>/dev/null )"; theirs=$?
    printf '%s' "$ourOut" > "$OUT/$name.ours.out"
    printf '%s' "$gccOut" > "$OUT/$name.gcc.out"

    if [ "$ours" = 124 ]; then
        echo "FAIL $name - our binary did not terminate within ${LIMIT}s"; exit 1
    fi
    if [ "$theirs" = 124 ]; then
        echo "FAIL $name - gcc's binary did not terminate within ${LIMIT}s (the case is wrong)"; exit 1
    fi
    if [ "$ours" != "$theirs" ]; then
        echo "FAIL $name - cc1 gave $ours, gcc gave $theirs"; exit 1
    fi
    if [ "$ours" != "$expected" ]; then
        echo "FAIL $name - both gave $ours, the case expects $expected"; exit 1
    fi
    if ! diff -q "$OUT/$name.ours.out" "$OUT/$name.gcc.out" >/dev/null; then
        echo "FAIL $name - same exit status, different output:"
        diff "$OUT/$name.gcc.out" "$OUT/$name.ours.out" | head -6 | sed 's/^/       /'
        exit 1
    fi
    exit 0
fi

# --- the whole suite -------------------------------------------------------

FILTER="${1:-}"
JOBS="${2:-$(nproc 2>/dev/null || echo 2)}"

[ -x "$CC1" ] || { echo "FATAL: $CC1 not built - run ./build first"; exit 1; }
mkdir -p "$OUT"

cases=()
for case_file in "$ROOT"/tests/cases/*.c; do
    name="$(basename "$case_file" .c)"
    [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue
    cases+=("$case_file")
done

if [ ${#cases[@]} -eq 0 ]; then echo "no cases match '$FILTER'"; exit 1; fi

verdicts="$(mktemp -d)"
trap 'rm -rf "$verdicts"' EXIT

# Each case writes its own verdict file, so nothing is interleaved and nothing
# is shared but the filesystem.
printf '%s\n' "${cases[@]}" | xargs -P "$JOBS" -I{} \
    bash -c 'f="{}"; n="$(basename "$f" .c)";
             out="$("'"$0"'" --one "$f" 2>&1)"; s=$?;
             printf "%s\n" "$out" > "'"$verdicts"'/$n";
             [ $s -eq 0 ] && : > "'"$verdicts"'/$n.ok"'

pass=0
fail=0
for case_file in "${cases[@]}"; do
    name="$(basename "$case_file" .c)"
    if [ -f "$verdicts/$name.ok" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        [ -s "$verdicts/$name" ] && cat "$verdicts/$name"
    fi
done

echo
echo "PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
