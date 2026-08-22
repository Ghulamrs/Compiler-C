#!/bin/sh
# The x86_64-windows backend on Windows, through Microsoft's own toolchain.
#
# tests/windows-native.sh runs the same corpus there through clang, which reads
# the GNU syntax cc1 used to write for every target. This one runs it the way
# the platform does: cc1 emits MASM, ml64 assembles it, link.exe links it
# against the static CRT, and the result is a PE binary with nothing in its
# path borrowed from another toolchain.
#
# Keep both, and not out of caution. They answer different questions. That one
# asks whether the *instruction selection* is right, on an assembler that reads
# the same syntax the Linux suite checks. This one asks whether the *spelling*
# is right - whether the translation in src/backend/Masm.cpp says to ml64 what
# the generator meant - and a wrong answer here is a wrong answer nowhere else,
# because no other assembler ever sees this text.
#
# It runs from the Mac, for the same reason tests/arm64.sh does: the Mac is the
# machine that can reach the Windows host over ssh, named "windows" in
# ~/.ssh/config. The Linux box cannot - it is in AWS and the Windows machine is
# on the LAN.
#
#   here      cc1 -arch x86_64-windows, one .asm per case, MASM by default
#   there     ml64 assembles, link.exe links against libcmt and the UCRT,
#             and the program runs
#
# Exits non-zero if any case fails, or if the Windows host cannot be reached.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1.exe"
SRC="$ROOT/tests/windows"
OUT="$ROOT/tests/out-masm-native"
HOST=${WINDOWS_HOST:-windows}
REMOTE='C:/cc1work/masm'

if [ ! -x "$CC1" ]; then
    echo "masm-native.sh: no cc1 here - run make first"
    exit 2
fi

if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST" 'exit 0' 2>/dev/null; then
    echo "masm-native.sh: cannot reach the Windows host '$HOST' over ssh."
    echo "  It needs a Host entry in ~/.ssh/config with a key that has no"
    echo "  passphrase, and OpenSSH Server running there. Set WINDOWS_HOST to"
    echo "  use a different name."
    exit 2
fi

rm -rf "$OUT" && mkdir -p "$OUT"

refused=0
for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)
    cp "$src" "$OUT/$name.c"
    if ! "$CC1" -S -arch x86_64-windows "$src" -o "$OUT/$name.asm" 2> "$OUT/$name.err"; then
        echo "FAIL $name - cc1 refused it:"
        sed 's/^/       /' "$OUT/$name.err"
        refused=$((refused + 1))
        rm -f "$OUT/$name.asm"
    fi
    # The hand-written callers, in MASM rather than the GNU syntax their .S
    # twins use. Renamed on the way over because this directory and the
    # generated .asm share a case-insensitive filesystem.
    [ -f "$SRC/$name.asm" ] && cp "$SRC/$name.asm" "$OUT/$name.harness.asm"
done
rm -f "$OUT"/*.err

ssh -o BatchMode=yes "$HOST" "Remove-Item -Recurse -Force '$REMOTE' -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force '$REMOTE' | Out-Null" || exit 2
scp -q -o BatchMode=yes "$OUT"/* "$HOST:$REMOTE/" || exit 2
scp -q -o BatchMode=yes "$SRC/masm-native.ps1" "$HOST:C:/cc1work/masm-native.ps1" || exit 2

ssh -o BatchMode=yes "$HOST" "powershell -ExecutionPolicy Bypass -File C:\\cc1work\\masm-native.ps1 -Dir '$REMOTE'"
status=$?

[ "$refused" -gt 0 ] && exit 1
exit $status
