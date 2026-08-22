#!/bin/sh
# The x86_64-windows backend, on Windows.
#
# tests/windows.sh runs the same corpus on Linux, and explains at length why
# that is sound. It is sound, and it is not the same thing as this. That suite
# proves the backend is self-consistent; this one proves it agrees with
# Microsoft.
#
# Keep both, and not out of caution. This suite is the weaker of the two on at
# least one rule, which was measured rather than assumed: setting the scratch
# register back to %rdi - the register Microsoft x64 makes the callee's to give
# back, and the whole reason the generator uses %r10 here - leaves all nine
# cases passing on real Windows. The C runtime happens to keep nothing in %rdi
# across its call to main, so nothing notices. The "// forbid:" line in
# w_callee_saved.c catches it every time, by reading the assembly, on Linux.
# A binary that runs is evidence, not proof.
#
# It runs from the Mac, for the same reason tests/arm64.sh does - the Mac is
# the machine that can reach what is needed. Here that is a Windows host over
# ssh, named "windows" in ~/.ssh/config. The Linux box cannot reach it; the box
# is in AWS and the Windows machine is on the LAN.
#
# The division of labour:
#
#   here      cc1 -arch x86_64-windows, one .s per case
#   there     clang assembles the AT&T syntax and links it as a PE binary,
#             against the real C runtime, and runs it
#
# clang rather than MSVC, and this is not a preference. cl compiles C and ml64
# assembles MASM; neither reads the GNU syntax cc1 writes. clang's integrated
# assembler does, targets x86_64-pc-windows-msvc, and links through the MSVC
# toolchain that is already on that machine.
#
# One transfer quirk, and it cost a confusing half hour. The Mac's filesystem
# is case-insensitive, so a harness named msabi_slots.S and the generated
# msabi_slots.s are the same file here and one silently replaces the other.
# The harnesses are relayed as .harness.asm for that reason alone.
#
#   ./tests/windows-native.sh
#
# Exits non-zero if any case fails, or if the Windows host cannot be reached.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1.exe"
SRC="$ROOT/tests/windows"
OUT="$ROOT/tests/out-windows-native"
HOST=${WINDOWS_HOST:-windows}
REMOTE='C:/cc1work/suite'

if [ ! -x "$CC1" ]; then
    echo "windows-native.sh: no cc1 here - run make first"
    exit 2
fi

if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST" 'exit 0' 2>/dev/null; then
    echo "windows-native.sh: cannot reach the Windows host '$HOST' over ssh."
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
    if ! "$CC1" -S -arch x86_64-windows -masm=gnu "$src" -o "$OUT/$name.s" 2> "$OUT/$name.err"; then
        echo "FAIL $name - cc1 refused it:"
        sed 's/^/       /' "$OUT/$name.err"
        refused=$((refused + 1))
        rm -f "$OUT/$name.s"
    fi
    # Not .S: this directory is on a case-insensitive filesystem, where that
    # name and the .s just written are the same file.
    [ -f "$SRC/$name.S" ] && cp "$SRC/$name.S" "$OUT/$name.harness.asm"
done
rm -f "$OUT"/*.err

ssh -o BatchMode=yes "$HOST" "Remove-Item -Recurse -Force '$REMOTE' -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force '$REMOTE' | Out-Null" || exit 2
scp -q -o BatchMode=yes "$OUT"/* "$HOST:$REMOTE/" || exit 2
scp -q -o BatchMode=yes "$SRC/windows-native.ps1" "$HOST:C:/cc1work/windows-native.ps1" || exit 2

ssh -o BatchMode=yes "$HOST" "powershell -ExecutionPolicy Bypass -File C:\\cc1work\\windows-native.ps1 -Dir '$REMOTE'"
status=$?

[ "$refused" -gt 0 ] && exit 1
exit $status
