#!/usr/bin/env bash
#
# The driver's own modes, which every other suite skips.
#
# run.sh, the probes and the fingerprint all invoke 'cc1 -S' and hand the
# assembly to gcc themselves. That leaves the two paths where cc1 drives the
# host toolchain - '-c', which assembles, and the bare form, which assembles
# and links - exercised by nothing. A bug there is invisible to a green suite.
#
# It was not hypothetical. An atexit handler registered against the Driver ran
# after main's locals were destroyed, so every 'cc1 -c' died of a bus error
# *after* writing a correct object and printing nothing. The whole suite passed
# because the whole suite uses -S.
#
# So this checks the things a user does and the suites do not: the exit status
# of each mode, that the temporaries are gone afterwards on success and on
# failure, and that a bare invocation produces a program that runs.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC1="${CC1:-$ROOT/cc1.exe}"
WORK="$ROOT/tests/out-driver"
TMP="${TMPDIR:-/tmp}"

[ -x "$CC1" ] || { echo "FATAL: $CC1 not built"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
pass=0; fail=0
check() {                       # check <what> <got> <want>
    if [ "$2" = "$3" ]; then pass=$((pass+1))
    else fail=$((fail+1)); echo "  FAIL $1: got '$2', want '$3'"; fi
}
leftovers() { ls "$TMP"/cc1-*.s 2>/dev/null | wc -l | tr -d ' '; }

cat > "$WORK/a.c" <<'EOF'
#include <stdio.h>
int twice(int n);
int main(void) { printf("%d\n", twice(21)); return 0; }
EOF
cat > "$WORK/b.c" <<'EOF'
int twice(int n) { return n + n; }
EOF
cat > "$WORK/bad.c" <<'EOF'
int main(void) { return no_such_name; }
EOF

rm -f "$TMP"/cc1-*.s 2>/dev/null

"$CC1" -S "$WORK/a.c" -o "$WORK/a.s" 2>/dev/null; check "-S status" "$?" "0"

"$CC1" -c "$WORK/a.c" -o "$WORK/a.o" 2>/dev/null; check "-c status" "$?" "0"
[ -f "$WORK/a.o" ] && check "-c wrote an object" "yes" "yes" \
                   || check "-c wrote an object" "no" "yes"
check "-c left no temporaries" "$(leftovers)" "0"

"$CC1" -c "$WORK/a.c" "$WORK/b.c" 2>/dev/null; check "-c two files" "$?" "0"
check "-c two left no temporaries" "$(leftovers)" "0"
rm -f a.o b.o

# A compile that fails still has to clean up after itself.
"$CC1" -c "$WORK/a.c" "$WORK/bad.c" >/dev/null 2>&1
check "failed -c reports failure" "$?" "1"
check "failed -c left no temporaries" "$(leftovers)" "0"

# The bare form: assemble, link, and the program must run.
"$CC1" "$WORK/a.c" "$WORK/b.c" -o "$WORK/prog" 2>/dev/null
check "link status" "$?" "0"
if [ -x "$WORK/prog" ]; then
    check "the program's output" "$("$WORK/prog" 2>/dev/null)" "42"
else
    fail=$((fail+1)); echo "  FAIL no program was linked"
fi
check "link left no temporaries" "$(leftovers)" "0"

# -S with one input and no -o writes to stdout, and must not be confused by an
# option's *value* looking like a second input.
out=$("$CC1" -S "$WORK/b.c" -arch "$($CC1 -arch 2>&1 | grep -oE 'x86_64-linux|arm64-darwin' | head -1)" 2>/dev/null | head -1)
case "$out" in
    ""|*"No such"*) fail=$((fail+1)); echo "  FAIL -S with -arch wrote no stdout" ;;
    *) pass=$((pass+1)) ;;
esac

rm -rf "$WORK"
echo ""
echo "driver modes  PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
