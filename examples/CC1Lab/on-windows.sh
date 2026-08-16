#!/usr/bin/env bash
#
# Build and run CC1Lab for x86_64-windows, on Windows.
#
# The Xcode project builds this lab for arm64-darwin with cc1 behind the shim.
# This is the other half: the same three files compiled for the other target,
# assembled by ml64 and linked by link.exe against the real UCRT, and run on a
# Windows machine over ssh.
#
# Nothing in the lab is written twice for it. <setjmp.h> is what differs
# between the two - the UCRT's setjmp takes a hidden frame argument and wants a
# sixteen-byte aligned jmp_buf, where the Unixes want neither - and all of that
# is inside the shipped header and the compiler. The source is the source.
#
# The rule that governs the shape of this script: cc1 has no assembler and no
# linker of its own, and nothing on a Mac can assemble MASM. So the work splits
# in two - the compile happens here, and the assemble, link and run happen
# there, which is why the assembly is relayed rather than the program.
#
#   ./examples/CC1Lab/on-windows.sh
#   WINDOWS_HOST=other ./examples/CC1Lab/on-windows.sh
#
# The host needs an entry in ~/.ssh/config with a key that has no passphrase,
# and Visual Studio installed - vswhere is what finds it. See
# tests/masm-native.sh, which does the same thing for the Windows corpus.

set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CC1="$ROOT/cc1"
HOST=${WINDOWS_HOST:-windows}
REMOTE='C:/cc1work/cc1lab'
OUT="$HERE/.windows-out"

SOURCES="main.c risky.c"

if [ ! -x "$CC1" ]; then
    echo "on-windows.sh: no cc1 at $CC1 - run make first"
    exit 2
fi

if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST" 'exit 0' 2>/dev/null; then
    echo "on-windows.sh: cannot reach the Windows host '$HOST' over ssh."
    echo "  Set WINDOWS_HOST, or add a Host entry to ~/.ssh/config."
    exit 2
fi

rm -rf "$OUT" && mkdir -p "$OUT"

for src in $SOURCES; do
    if ! "$CC1" -S -arch x86_64-windows "$HERE/$src" -o "$OUT/${src%.c}.asm" \
         2> "$OUT/${src%.c}.err"; then
        echo "on-windows.sh: cc1 refused $src:"
        sed 's/^/       /' "$OUT/${src%.c}.err"
        exit 1
    fi
done

ssh -o BatchMode=yes "$HOST" \
    "Remove-Item -Recurse -Force '$REMOTE' -ErrorAction SilentlyContinue; \
     New-Item -ItemType Directory -Force '$REMOTE' | Out-Null" || exit 2

scp -q -o BatchMode=yes "$OUT"/*.asm "$HOST:$REMOTE/" || exit 2
scp -q -o BatchMode=yes "$HERE/on-windows.ps1" "$HOST:C:/cc1work/cc1lab.ps1" || exit 2

ssh -o BatchMode=yes "$HOST" \
    "powershell -ExecutionPolicy Bypass -File C:\\cc1work\\cc1lab.ps1 -Dir '$REMOTE'"
