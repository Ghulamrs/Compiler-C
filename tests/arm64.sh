#!/bin/sh
# The arm64-darwin backend, run the same way the x86-64 one is: compile each
# case twice, once with cc1 and once with the reference compiler, run both, and
# require that they agree on the printed output and the exit status.
#
# This runs on the Mac rather than the box, because the Mac is arm64 and can
# execute what this backend emits. clang is the reference here, where gcc is the
# reference on Linux - a different compiler, but the same argument: it is the
# implementation sitting on the same disk, and where the two disagree the case
# is wrong until the standard says otherwise.
#
# It is a subset. The backend still refuses structs, member access, calls
# through a function pointer and va_start by name, so tests/cases is not the
# corpus here - tests/arm64/ is, and it grows as the backend does. Floating
# point, postfix and switch have since landed and have cases below.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1"
SRC="$ROOT/tests/arm64"
OUT="$ROOT/tests/out-arm64"

if [ "$(uname -m)-$(uname -s)" != "arm64-Darwin" ]; then
    echo "arm64.sh: this must run on an arm64 Mac; it is $(uname -m)-$(uname -s)"
    exit 1
fi

rm -rf "$OUT" && mkdir -p "$OUT"
pass=0
fail=0

for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)
    expect=$(sed -n 's|^// expect: *||p' "$src" | head -1)

    if ! "$CC1" -S -arch arm64-darwin "$src" -o "$OUT/$name.s" 2> "$OUT/$name.cc1.err"; then
        echo "FAIL $name - cc1 refused it:"
        sed 's/^/       /' "$OUT/$name.cc1.err"
        fail=$((fail + 1))
        continue
    fi
    if ! clang "$OUT/$name.s" -o "$OUT/$name.ours" -lm 2> "$OUT/$name.as.err"; then
        echo "FAIL $name - the assembler refused what cc1 emitted:"
        sed 's/^/       /' "$OUT/$name.as.err" | head -5
        fail=$((fail + 1))
        continue
    fi
    clang -w "$src" -o "$OUT/$name.ref" -lm 2> /dev/null

    ours_out=$("$OUT/$name.ours" 2>&1); ours_rc=$?
    ref_out=$("$OUT/$name.ref" 2>&1);  ref_rc=$?

    if [ "$ours_out" != "$ref_out" ] || [ "$ours_rc" != "$ref_rc" ]; then
        echo "FAIL $name - disagrees with clang"
        echo "       ours: rc=$ours_rc out=[$ours_out]"
        echo "       ref : rc=$ref_rc out=[$ref_out]"
        fail=$((fail + 1))
        continue
    fi
    if [ -n "$expect" ] && [ "$ours_rc" != "$expect" ]; then
        echo "FAIL $name - both agree on $ours_rc, but the case expects $expect"
        fail=$((fail + 1))
        continue
    fi
    pass=$((pass + 1))
done

echo
echo "arm64-darwin  PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
