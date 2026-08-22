#!/usr/bin/env bash
#
# cc1's objects linked against the host compiler's, both directions.
#
# This exists because of a bug the rest of the suite could not see. A struct of
# four bytes was returned in %rdx where the ABI says %rax - and cc1's caller
# read %rdx too, so cc1 agreed with itself perfectly. tests/run.sh compiles a
# case twice and compares what the two programs print; two separately built,
# internally consistent programs print the same thing, and the disagreement
# never surfaces. It took building for a target whose other half was right for
# the two to fall out of step.
#
# So the question here is not "does cc1 produce the right answer" but "does
# cc1 produce the answer the *other* compiler expects". That can only be asked
# by making one of them the caller and the other the callee, and it is the only
# check in this repository that links objects from two different compilers into
# one program.
#
# Each case below is a pair: an interface, a callee compiled by one compiler,
# and a caller compiled by the other. Both directions are run, because getting
# them wrong in the same way is exactly the failure this is here to catch.
#
#   ./tests/cross-abi.sh
#
# Exits non-zero if any case fails.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC1="$ROOT/cc1.exe"
REF="${CC1_CC:-cc}"
OUT="$ROOT/tests/out-cross"

[ -x "$CC1" ] || { echo "FATAL: $CC1 not built - run ./build first"; exit 1; }

# A runaway case should be killed rather than hang the suite, but 'timeout' is
# GNU coreutils and a Mac has neither it nor gtimeout unless one was installed.
# Without this the command simply did not exist, every case ran to an empty
# string, and all eight failed here while passing on Linux - a missing utility
# wearing an ABI bug's clothes. Degrade to running uncapped rather than skip
# the suite: an unbounded case is a worse outcome than no case only if it
# hangs, and none of these loops.
LIMIT=5
if command -v timeout >/dev/null 2>&1;   then CAP="timeout $LIMIT"
elif command -v gtimeout >/dev/null 2>&1; then CAP="gtimeout $LIMIT"
else CAP=""; echo "cross-abi.sh: no timeout here - running uncapped"; fi

rm -rf "$OUT" && mkdir -p "$OUT"
pass=0
fail=0

# A case is a name, a shared header, the callee's source, and the caller's.
# The caller prints; the expectation is what it must print.
run_case() {
    local name="$1" expect="$2"

    for direction in "cc1-callee" "cc1-caller"; do
        local ourObj theirObj
        if [ "$direction" = "cc1-callee" ]; then
            "$CC1" -c "$OUT/$name.callee.c" -o "$OUT/$name.callee.o" 2>"$OUT/$name.err" || {
                echo "FAIL $name/$direction - cc1 refused the callee"; sed 's/^/       /' "$OUT/$name.err"
                fail=$((fail + 1)); continue; }
            $REF -w -c "$OUT/$name.caller.c" -o "$OUT/$name.caller.o" 2>>"$OUT/$name.err" || {
                echo "FAIL $name/$direction - the reference refused the caller"
                fail=$((fail + 1)); continue; }
        else
            $REF -w -c "$OUT/$name.callee.c" -o "$OUT/$name.callee.o" 2>"$OUT/$name.err" || {
                echo "FAIL $name/$direction - the reference refused the callee"
                fail=$((fail + 1)); continue; }
            "$CC1" -c "$OUT/$name.caller.c" -o "$OUT/$name.caller.o" 2>>"$OUT/$name.err" || {
                echo "FAIL $name/$direction - cc1 refused the caller"; sed 's/^/       /' "$OUT/$name.err"
                fail=$((fail + 1)); continue; }
        fi

        if ! $REF "$OUT/$name.caller.o" "$OUT/$name.callee.o" -o "$OUT/$name.$direction" \
             -lm 2>>"$OUT/$name.err"; then
            echo "FAIL $name/$direction - link failed"
            sed 's/^/       /' "$OUT/$name.err"
            fail=$((fail + 1)); continue
        fi

        local got
        got="$( $CAP "$OUT/$name.$direction" 2>/dev/null )"
        if [ "$got" = "$expect" ]; then
            pass=$((pass + 1))
        else
            echo "FAIL $name/$direction"
            echo "       expected: $expect"
            echo "       got:      $got"
            fail=$((fail + 1))
        fi
    done
}

# --- the cases -------------------------------------------------------------
# Struct returns at every size that changes how the ABI moves them: inside one
# register, inside two, and past the limit where it becomes a hidden pointer.
# The four-byte one is the case that was wrong.

cat > "$OUT/small.callee.c" <<'EOF'
struct S { char c; short s; };
struct S mks(char c, short s) { struct S v; v.c = c; v.s = s; return v; }
int sums(struct S v) { return v.c + v.s; }
EOF
cat > "$OUT/small.caller.c" <<'EOF'
#include <stdio.h>
struct S { char c; short s; };
struct S mks(char c, short s);
int sums(struct S v);
int main(void) {
    struct S s = mks(65, 300);
    printf("%d %d %d\n", s.c, s.s, sums(s));
    return 0;
}
EOF
run_case small "65 300 365"

cat > "$OUT/two.callee.c" <<'EOF'
struct T { long a; long b; };
struct T mkt(long a, long b) { struct T v; v.a = a; v.b = b; return v; }
long sumt(struct T v) { return v.a + v.b; }
EOF
cat > "$OUT/two.caller.c" <<'EOF'
#include <stdio.h>
struct T { long a; long b; };
struct T mkt(long a, long b);
long sumt(struct T v);
int main(void) {
    struct T t = mkt(1000000, 2000000);
    printf("%ld %ld %ld\n", t.a, t.b, sumt(t));
    return 0;
}
EOF
run_case two "1000000 2000000 3000000"

cat > "$OUT/big.callee.c" <<'EOF'
struct B { int a; int b; int c; int d; int e; };
struct B mkb(int n) { struct B v; v.a=n; v.b=n+1; v.c=n+2; v.d=n+3; v.e=n+4; return v; }
int sumb(struct B v) { return v.a + v.b + v.c + v.d + v.e; }
EOF
cat > "$OUT/big.caller.c" <<'EOF'
#include <stdio.h>
struct B { int a; int b; int c; int d; int e; };
struct B mkb(int n);
int sumb(struct B v);
int main(void) {
    struct B b = mkb(10);
    printf("%d %d %d\n", b.a, b.e, sumb(b));
    return 0;
}
EOF
run_case big "10 14 60"

# A mixed eightbyte: an int beside a float is one INTEGER and one SSE lane
# under System V, which is the classification most easily got wrong.
cat > "$OUT/mixed.callee.c" <<'EOF'
struct M { int n; double d; };
struct M mkm(int n, double d) { struct M v; v.n = n; v.d = d; return v; }
double summ(struct M v) { return v.n + v.d; }
EOF
cat > "$OUT/mixed.caller.c" <<'EOF'
#include <stdio.h>
struct M { int n; double d; };
struct M mkm(int n, double d);
double summ(struct M v);
int main(void) {
    struct M m = mkm(7, 0.5);
    printf("%d %.1f %.1f\n", m.n, m.d, summ(m));
    return 0;
}
EOF
run_case mixed "7 0.5 7.5"

echo
if [ "$fail" -eq 0 ]; then
    echo "cross-ABI against $REF  PASS: $pass   FAIL: 0"
else
    echo "cross-ABI against $REF  PASS: $pass   FAIL: $fail"
fi
[ "$fail" -eq 0 ]
