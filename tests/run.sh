#!/usr/bin/env bash
#
# Differential regression suite.
#
# Every case is compiled twice - once by cc1, once by gcc - and both binaries
# are run. A case passes only when the two agree AND both match the exit code
# written at the top of the file.
#
# Comparing against gcc rather than against expectations alone is the point.
# An expectation is my opinion about C; gcc is the reference implementation
# sitting on the same disk. Where the two disagree the case is wrong until
# proven otherwise, and the standard is the tie-breaker.
#
# Every binary runs under a timeout. Once the language has loops, a codegen
# bug does not merely give a wrong answer - it gives no answer at all. Breaking
# the remainder fixup so that % returned the quotient turned collatz.c into an
# infinite loop, and without a limit here the suite hung and left a process
# spinning at 100% CPU on a burstable instance until it was killed by hand.
#
#   ./tests/run.sh            all cases
#   ./tests/run.sh gcd        cases whose name contains "gcd"
#
# Exits non-zero if any case fails, so it drops into a git hook or CI.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC1="$ROOT/cc1"
OUT="$ROOT/tests/out"
FILTER="${1:-}"
LIMIT=5          # seconds any one compiled program may run

[ -x "$CC1" ] || { echo "FATAL: $CC1 not built - run ./build first"; exit 1; }
mkdir -p "$OUT"

pass=0
fail=0

for case in "$ROOT"/tests/cases/*.c; do
    name="$(basename "$case" .c)"
    [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue

    expected="$(sed -n '1s|^// expect: *\([0-9-]*\).*|\1|p' "$case")"
    if [ -z "$expected" ]; then
        echo "FAIL $name - no '// expect: N' on line 1"
        fail=$((fail + 1))
        continue
    fi

    # Ours: cc1 emits assembly, gcc assembles and links it.
    if ! "$CC1" "$case" -o "$OUT/$name.s" 2> "$OUT/$name.err"; then
        echo "FAIL $name - cc1 rejected it:"
        sed 's/^/       /' "$OUT/$name.err"
        fail=$((fail + 1))
        continue
    fi
    if ! gcc "$OUT/$name.s" -o "$OUT/$name.ours" 2>> "$OUT/$name.err"; then
        echo "FAIL $name - our assembly would not assemble:"
        sed 's/^/       /' "$OUT/$name.err"
        fail=$((fail + 1))
        continue
    fi

    # Theirs: the reference on the same disk.
    if ! gcc -w "$case" -o "$OUT/$name.gcc" 2>> "$OUT/$name.err"; then
        echo "FAIL $name - gcc rejected the case itself (the case is wrong)"
        fail=$((fail + 1))
        continue
    fi

    timeout "$LIMIT" "$OUT/$name.ours"; ours=$?
    timeout "$LIMIT" "$OUT/$name.gcc";  theirs=$?

    # 124 is what timeout reports when it had to kill the program. Distinguished
    # from a wrong answer because it usually means a loop that never ends.
    if [ "$ours" = 124 ]; then
        echo "FAIL $name - our binary did not terminate within ${LIMIT}s"
        fail=$((fail + 1))
    elif [ "$theirs" = 124 ]; then
        echo "FAIL $name - gcc's binary did not terminate within ${LIMIT}s (the case is wrong)"
        fail=$((fail + 1))
    elif [ "$ours" != "$theirs" ]; then
        echo "FAIL $name - cc1 gave $ours, gcc gave $theirs"
        fail=$((fail + 1))
    elif [ "$ours" != "$expected" ]; then
        echo "FAIL $name - both gave $ours, the case expects $expected"
        fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
done

echo
echo "PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
