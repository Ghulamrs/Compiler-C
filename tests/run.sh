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

# Runs the two binaries and reports the verdict. Shared by both modes below,
# because a comparison written twice is a comparison that will differ twice.
#
# Captured through command substitution rather than redirected to a file, which
# is what keeps the shell's own "Segmentation fault" notice out of the report:
# that notice is printed by whichever shell waits for the program, so
# redirecting the program's stderr - or a subshell's - does not silence it. A
# crash is already legible as an exit status of 139.
#
# Command substitution drops trailing newlines. Both sides are captured the same
# way, so the comparison stays like for like.
compare() {
    local name="$1" expected="$2"
    local ourOut gccOut ours theirs

    ourOut="$( timeout "$LIMIT" "$OUT/$name.ours" 2>/dev/null )"; ours=$?
    gccOut="$( timeout "$LIMIT" "$OUT/$name.gcc"  2>/dev/null )"; theirs=$?
    printf '%s' "$ourOut" > "$OUT/$name.ours.out"
    printf '%s' "$gccOut" > "$OUT/$name.gcc.out"

    if [ "$ours" = 124 ]; then
        echo "FAIL $name - our binary did not terminate within ${LIMIT}s"; return 1
    fi
    if [ "$theirs" = 124 ]; then
        echo "FAIL $name - gcc's binary did not terminate within ${LIMIT}s (the case is wrong)"; return 1
    fi
    if [ "$ours" != "$theirs" ]; then
        echo "FAIL $name - cc1 gave $ours, gcc gave $theirs"; return 1
    fi
    if [ "$ours" != "$expected" ]; then
        echo "FAIL $name - both gave $ours, the case expects $expected"; return 1
    fi
    if ! diff -q "$OUT/$name.ours.out" "$OUT/$name.gcc.out" >/dev/null; then
        echo "FAIL $name - same exit status, different output:"
        diff "$OUT/$name.gcc.out" "$OUT/$name.ours.out" | head -6 | sed 's/^/       /'
        return 1
    fi
    return 0
}

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

    if ! "$CC1" -S "$case_file" -o "$OUT/$name.s" 2> "$OUT/$name.err"; then
        echo "FAIL $name - cc1 rejected it:"; sed 's/^/       /' "$OUT/$name.err"; exit 1
    fi
    if ! gcc "$OUT/$name.s" -o "$OUT/$name.ours" -lm 2>> "$OUT/$name.err"; then
        echo "FAIL $name - our assembly would not assemble:"
        sed 's/^/       /' "$OUT/$name.err"; exit 1
    fi
    if ! gcc -w "$case_file" -o "$OUT/$name.gcc" -lm 2>> "$OUT/$name.err"; then
        echo "FAIL $name - gcc rejected the case itself (the case is wrong)"; exit 1
    fi

    compare "$name" "$expected" || exit 1
    exit 0
fi

# --- one multi-file case ---------------------------------------------------
# A directory of sources compiled separately and linked, which is the only way
# to test the thing C is built around: a translation unit knows nothing of its
# neighbours until the linker joins them. Nothing in tests/cases can reach it -
# every case there is one file - so separate compilation went untested from the
# day it started working until this existed.
#
# The expectation lives on the first line of main.c, the same place as always.
if [ "${1:-}" = "--one-multi" ]; then
    dir="$2"
    name="multi.$(basename "$dir")"
    main="$dir/main.c"

    if [ ! -f "$main" ]; then
        echo "FAIL $name - no main.c in it"; exit 1
    fi
    expected="$(sed -n '1s|^// expect: *\([0-9-]*\).*|\1|p' "$main")"
    if [ -z "$expected" ]; then
        echo "FAIL $name - no '// expect: N' on line 1 of main.c"; exit 1
    fi

    srcs=()
    for f in "$dir"/*.c; do srcs+=("$f"); done

    : > "$OUT/$name.err"
    asm=()
    for src in "${srcs[@]}"; do
        base="$(basename "$src" .c)"
        # One invocation per file on purpose: each unit is compiled knowing
        # nothing of the others, which is the property under test.
        if ! "$CC1" -S "$src" -o "$OUT/$name.$base.s" 2>> "$OUT/$name.err"; then
            echo "FAIL $name - cc1 rejected $(basename "$src"):"
            sed 's/^/       /' "$OUT/$name.err"; exit 1
        fi
        asm+=("$OUT/$name.$base.s")
    done

    if ! gcc "${asm[@]}" -o "$OUT/$name.ours" -lm 2>> "$OUT/$name.err"; then
        echo "FAIL $name - our units would not assemble and link together:"
        sed 's/^/       /' "$OUT/$name.err"; exit 1
    fi
    if ! gcc -w "${srcs[@]}" -o "$OUT/$name.gcc" -lm 2>> "$OUT/$name.err"; then
        echo "FAIL $name - gcc rejected the case itself (the case is wrong)"; exit 1
    fi

    compare "$name" "$expected" || exit 1
    exit 0
fi

# --- the parallel job loop -------------------------------------------------
# One invocation compiling many files at once must produce exactly what the
# serial loop produces, byte for byte. Nothing in tests/cases can reach this:
# every case there is a separate invocation of cc1 with a single input, so the
# threaded path would otherwise go untested from the moment it was written.
#
# The corpus is copied out of tests/cases first. With no -o each input writes
# its .s beside itself, and 361 of those landing in tests/cases would be 361
# files nobody asked for.
#
# It also checks that threads ran at all. That did not used to be true: making
# threadCount() return 1 unconditionally left this passing, because both runs
# were then the serial loop agreeing with itself. Timing cannot settle it at a
# millisecond a unit, so cc1 -time now reports the decision and this greps for
# it.
if [ "${1:-}" = "--one-parallel" ]; then
    name="parallel.determinism"
    src="$OUT/par"
    rm -rf "$src" "$OUT/par.serial" "$OUT/par.threaded"
    mkdir -p "$src" "$OUT/par.serial" "$OUT/par.threaded"
    # The headers as well as the sources. A case that includes "pp_helper.h"
    # resolves it beside itself, and beside itself is now this directory.
    cp "$ROOT"/tests/cases/*.c "$ROOT"/tests/cases/*.h "$src/"

    if ! "$CC1" -S -j 1 "$src"/*.c 2> "$OUT/$name.err"; then
        echo "FAIL $name - cc1 -j 1 rejected the corpus:"
        sed 's/^/       /' "$OUT/$name.err"; exit 1
    fi
    mv "$src"/*.s "$OUT/par.serial/"

    # -j 4 rather than the default. The default asks the machine how many cores
    # it has, and on a one-core box the honest answer is one thread - which
    # would make this check compare the serial loop with itself. An explicit -j
    # is taken as asked, so threads run wherever this suite runs.
    if ! "$CC1" -S -j 4 -time "$src"/*.c 2> "$OUT/$name.threaded.err"; then
        echo "FAIL $name - the threaded run rejected the corpus:"
        sed 's/^/       /' "$OUT/$name.threaded.err"; exit 1
    fi
    mv "$src"/*.s "$OUT/par.threaded/"

    # And that they were threads. -time reports the decision, so a threadCount()
    # that had become 1 is caught here rather than passing as two identical
    # serial runs - which is exactly what it did before this line existed.
    if ! grep -q "on 4 threads" "$OUT/$name.threaded.err"; then
        echo "FAIL $name - asked for 4 threads and did not get them:"
        grep "jobs on" "$OUT/$name.threaded.err" | sed 's/^/       /'; exit 1
    fi

    if ! diff -rq "$OUT/par.serial" "$OUT/par.threaded" > "$OUT/$name.diff" 2>&1; then
        echo "FAIL $name - threaded output differs from serial:"
        head -5 "$OUT/$name.diff" | sed 's/^/       /'; exit 1
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

for dir in "$ROOT"/tests/multi/*/; do
    [ -d "$dir" ] || continue
    name="multi.$(basename "$dir")"
    [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue
    cases+=("${dir%/}")
done

# parallel.determinism is not in the array - it is one check about the driver
# rather than a program to compile - so the filter has to reach it separately,
# and "no cases match" has to know it exists. Filtering to it alone is exactly
# what you want when the threaded loop is what you are debugging.
runParallel=0
if [ -z "$FILTER" ] || [[ "parallel.determinism" == *"$FILTER"* ]]; then
    runParallel=1
fi

if [ ${#cases[@]} -eq 0 ] && [ "$runParallel" -eq 0 ]; then
    echo "no cases match '$FILTER'"; exit 1
fi

verdicts="$(mktemp -d)"
trap 'rm -rf "$verdicts"' EXIT

# Each case writes its own verdict file, so nothing is interleaved and nothing
# is shared but the filesystem.
if [ ${#cases[@]} -gt 0 ]; then
printf '%s\n' "${cases[@]}" | xargs -P "$JOBS" -I{} \
    bash -c 'f="{}";
             if [ -d "$f" ]; then n="multi.$(basename "$f")"; mode="--one-multi";
             else n="$(basename "$f" .c)"; mode="--one"; fi
             out="$("'"$0"'" "$mode" "$f" 2>&1)"; s=$?;
             printf "%s\n" "$out" > "'"$verdicts"'/$n";
             [ $s -eq 0 ] && : > "'"$verdicts"'/$n.ok"'
fi

pass=0
fail=0
for case_file in "${cases[@]}"; do
    if [ -d "$case_file" ]; then name="multi.$(basename "$case_file")"
    else name="$(basename "$case_file" .c)"; fi
    if [ -f "$verdicts/$name.ok" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        [ -s "$verdicts/$name" ] && cat "$verdicts/$name"
    fi
done

# Last, and on its own, because it is the one check that is about the driver
# rather than about a program. It compiles the whole corpus twice in two
# invocations, so running it beside the parallel case loop would put the machine
# under a load that tells us nothing.
if [ "$runParallel" -eq 1 ]; then
    out="$("$0" --one-parallel 2>&1)"
    if [ $? -eq 0 ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf '%s\n' "$out"
    fi
fi

echo
echo "PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
