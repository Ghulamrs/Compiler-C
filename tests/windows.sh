#!/bin/sh
# The x86_64-windows backend, checked on Linux.
#
# This needs saying, because it looks wrong. There is no Windows machine here
# and no mingw, wine or clang on this box - and yet these binaries run, and the
# convention they follow really is Microsoft's rather than System V's.
#
# What makes it work is that a Windows-convention program with no library calls
# is a self-contained blob that Linux is happy to execute. The only boundary is
# main itself: it takes no arguments, and both conventions return int in %eax,
# so glibc calling it cannot tell the difference. Everything inside - which
# register holds argument three, where the fifth one sits, who opens the shadow
# space - is between functions cc1 emitted, and they agree with each other.
#
# One further thing has to hold, and it is a real property of the backend rather
# than a convenience: Microsoft x64 makes %rdi and %rsi the callee's to preserve
# where System V makes them scratch. The generator therefore uses %r10 as its
# scratch on this target, so a cc1 function clobbers neither, and glibc gets
# back what it lent. Break that and this suite keeps passing while real Windows
# code corrupts its caller - which is why w_callee_saved.c greps the assembly.
#
# Two kinds of case live here.
#
#   Ordinary ones are differential in the usual way: compiled by cc1 for
#   x86_64-windows, compiled again by gcc for Linux, both run, both compared.
#   That works because the two ABIs compute the same answers - they disagree
#   about where arguments travel, not about what addition is.
#
#   A case marked "// windows-only:" is compiled and checked but not run,
#   because it calls the C library and so crosses the very boundary this suite
#   depends on nothing crossing. tests/windows-native.sh runs it for real.
#
#   A case marked "// no-reference:" skips gcc. LLP64 is the reason: long is 4
#   bytes on this target and 8 to the gcc on this box, so for anything that
#   measures a long the reference is not a second opinion but a different
#   question. Those cases are held to their // expect: line alone, and the
#   marker has to say why.
#
# And one case is neither. msabi_slots.S is a caller written by hand from the
# Microsoft convention - it loads %rcx, %xmm1, %r8, %xmm3 for arguments one to
# four and opens 32 bytes of shadow - and it calls a function cc1 compiled. Both
# suites above would pass a convention that was wrong in the same way at both
# ends, because cc1 is on both ends. This one would not: the caller is an
# independent reading of the spec, and if cc1 puts argument three anywhere but
# %r8 the answer comes out wrong. Any case with a .S beside it is linked
# against it and gets no gcc reference.
#
#   ./tests/windows.sh
#
# Exits non-zero if any case fails.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1"
SRC="$ROOT/tests/windows"
OUT="$ROOT/tests/out-windows"

if [ "$(uname -m)-$(uname -s)" != "x86_64-Linux" ]; then
    echo "windows.sh: this assembles and runs x86-64, and it is $(uname -m)-$(uname -s)"
    exit 1
fi

rm -rf "$OUT" && mkdir -p "$OUT"
pass=0
fail=0

for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)
    expect=$(sed -n 's|^// expect: *||p' "$src" | head -1)
    noref=$(sed -n 's|^// no-reference: *||p' "$src" | head -1)
    # A case that calls the C library cannot run here at all. The trick this
    # suite rests on is that nothing crosses the boundary between conventions;
    # a Windows-convention call into glibc's System V printf is that boundary,
    # and it segfaults. Such a case is still compiled - the refusals and the
    # "// forbid:" checks are worth having - and is run by
    # tests/windows-native.sh, on Windows, where it means something.
    winonly=$(sed -n 's|^// windows-only: *||p' "$src" | head -1)
    harness="$SRC/$name.S"

    if ! "$CC1" -S -arch x86_64-windows "$src" -o "$OUT/$name.s" 2> "$OUT/$name.cc1.err"; then
        echo "FAIL $name - cc1 refused it:"
        sed 's/^/       /' "$OUT/$name.cc1.err"
        fail=$((fail + 1))
        continue
    fi

    if [ -f "$harness" ]; then
        link="$OUT/$name.s $harness"
    else
        link="$OUT/$name.s"
    fi
    # shellcheck disable=SC2086
    if ! gcc $link -o "$OUT/$name.ours" 2> "$OUT/$name.as.err"; then
        echo "FAIL $name - the assembler refused what cc1 emitted:"
        sed 's/^/       /' "$OUT/$name.as.err" | head -5
        fail=$((fail + 1))
        continue
    fi

    # Some rules leave no trace in the answer. A "// forbid:" line names a
    # string that must not appear in the emitted assembly, which is how the
    # callee-saved registers are held: nothing that runs on Linux notices a
    # Windows callee eating %rdi, so the text is the only witness there is.
    forbidden=""
    sed -n 's|^// forbid: *||p' "$src" | while read -r pattern; do
        [ -n "$pattern" ] || continue
        if grep -q -- "$pattern" "$OUT/$name.s"; then
            echo "$pattern"
        fi
    done > "$OUT/$name.forbidden"
    forbidden=$(cat "$OUT/$name.forbidden")
    if [ -n "$forbidden" ]; then
        echo "FAIL $name - the assembly uses what this case forbids:"
        echo "$forbidden" | sed 's/^/       /'
        fail=$((fail + 1))
        continue
    fi

    if [ -n "$winonly" ]; then
        pass=$((pass + 1))
        continue
    fi

    ours_out=$("$OUT/$name.ours" 2>&1); ours_rc=$?

    if [ -z "$noref" ] && [ ! -f "$harness" ]; then
        gcc -w "$src" -o "$OUT/$name.ref" 2> /dev/null
        ref_out=$("$OUT/$name.ref" 2>&1); ref_rc=$?
        if [ "$ours_out" != "$ref_out" ] || [ "$ours_rc" != "$ref_rc" ]; then
            echo "FAIL $name - disagrees with gcc"
            echo "       ours: rc=$ours_rc out=[$ours_out]"
            echo "       ref : rc=$ref_rc out=[$ref_out]"
            fail=$((fail + 1))
            continue
        fi
    fi

    if [ -n "$expect" ] && [ "$ours_rc" != "$expect" ]; then
        echo "FAIL $name - gave $ours_rc, the case expects $expect"
        fail=$((fail + 1))
        continue
    fi
    pass=$((pass + 1))
done

echo
echo "x86_64-windows  PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
