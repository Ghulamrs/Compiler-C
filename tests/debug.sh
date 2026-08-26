#!/bin/sh
# What -g is for, asked of the thing it is for: a debugger.
#
# Every other suite here reads what cc1 wrote. This one hands the program to
# gdb or lldb and asks where it stopped, because a line table is not right
# because it looks right - .loc directives can be perfectly formed and still
# describe nothing a debugger will use, which is exactly what happened while
# this was being written. The compile unit was missing and every breakpoint
# stayed pending; llvm-dwarfdump was happy throughout.
#
# It runs on both machines that can debug what they compile: the Mac drives
# lldb over arm64-darwin, the Linux box drives gdb over x86_64-linux. The
# two debuggers say different things and the greps below know the difference.
# x86_64-windows is not here because -g is refused there - MASM carries no
# line table - and that refusal is itself one of the checks.
#
# Each case carries what to ask about it:
#   // stop: N        break at line N, run, and be stopped at line N
#   // func: NAME L   break on a function by name, and land on line L, which
#                     is past its prologue rather than at its brace
#   // step: N M      break at N, step once, arrive at M
#   // bt: A B        stopped in A, a backtrace names both A and B
#   // with: NAME.c    compile tests/debug/parts/NAME.c into the program too
#   // stopin: F N     break at line N of file F - which is how a case asks
#                      about a file that is not the one holding main
#   // print: E V      stopped at the 'stop' line, printing E gives V
#
# The print directives of one case are answered in a single run, which is why
# every value a case expects is a different one: the check is that the value
# appears as the answer to something, and two expectations sharing a value
# could cover for each other.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="${CC1:-$ROOT/cc1.exe}"
SRC="$ROOT/tests/debug"
OUT="$ROOT/tests/out-debug"

# An optional target. With none it is the host's own, which is the debugger's
# native case. 'x86_64-windows' compiles this same corpus for the Microsoft
# ABI in the GNU spelling and debugs that - which Linux can do for the reason
# tests/windows.sh sets out at length: a Windows-convention program calling no
# library is a self-contained blob, and every case here calls none. Checked,
# not assumed - a case that grew a printf would run under the wrong ABI and
# fail here in a way that looks like a debug-information bug.
ARCH="${1:-}"
ARCHFLAGS=""
WHAT=""
if [ -n "$ARCH" ]; then
    ARCHFLAGS="-arch $ARCH -masm=gnu"
    WHAT=", $ARCH"
    OUT="$OUT-$(echo "$ARCH" | tr -d ' ')"
    # Comments stripped first. This codebase's cases carry long explanatory
    # headers, and one that merely *mentions* printf would otherwise abort the
    # whole run claiming it calls a library - a guard that cries wolf is worse
    # than none, because the next person turns it off.
    for c in "$SRC"/*.c; do
        if sed 's|//.*||' "$c" | grep -q -e '#include' -e 'printf'; then
            echo "debug.sh: $(basename "$c") calls a library, which $ARCH"
            echo "cannot do on this host - a Windows-convention program only"
            echo "runs here while it calls nothing. Keep the corpus self-"
            echo "contained, or stop running it for that target."
            exit 1
        fi
    done
fi

case "$(uname -s)" in
    Darwin) DBG=lldb ;;
    Linux)  DBG=gdb ;;
    *)      echo "debug.sh: no debugger known for $(uname -s)"; exit 1 ;;
esac
if ! command -v "$DBG" >/dev/null 2>&1; then
    echo "debug.sh: $DBG is not installed, and this suite is nothing without it"
    exit 1
fi

rm -rf "$OUT" && mkdir -p "$OUT"
pass=0
fail=0

report() {          # report <ok|no> <case> <what> [detail]
    if [ "$1" = ok ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL $2 - $3"
        [ $# -ge 4 ] && echo "$4" | sed 's/^/       /' | head -6
    fi
}

# One debugger session, its whole transcript on stdout. The commands differ
# between the two; everything that reads them is below.
debug() {           # debug <program> <break-spec> [more commands...]
    prog=$1; shift
    if [ "$DBG" = lldb ]; then
        set -- "$@"
        lldb -b -o "$1" -o run "$@" -o kill -- "$prog" 2>&1
    else
        gdb -batch -ex "$1" -ex run "$@" ./"$prog" 2>&1
    fi
}

for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)
    base=$(basename "$src")
    prog="$OUT/$name"
    # A case may be more than one file. The extra ones live in tests/debug/parts
    # so that this loop does not take them for cases; they have no main.
    with=$(sed -n 's|^// with: *||p' "$src" | head -1)
    extra=""
    [ -n "$with" ] && extra="$SRC/parts/$with"

    # The host's own target goes through the driver end to end, which is a
    # path worth exercising. A cross target cannot: cc1 declines to assemble
    # code for a machine it is not on, and is right to - so the assembly is
    # written and handed to the host assembler, exactly as tests/windows.sh
    # does. The debug information is cc1's either way; what gcc contributes
    # here is an assembler and a linker, not a line table.
    if [ -n "$ARCH" ]; then
        # One source at a time, because -S writes one .s per input and cannot
        # be given -o for more than one. Which is the same shape the real path
        # has anyway: a .o per source, and the linker afterwards.
        if ! "$CC1" -S -g $ARCHFLAGS "$src" -o "$OUT/$name.s" \
                2> "$OUT/$name.cc1.err"; then
            report no "$name" "cc1 -g refused it" "$(cat "$OUT/$name.cc1.err")"
            continue
        fi
        asmextra=""
        if [ -n "$extra" ]; then
            asmextra="$OUT/$name.other.s"
            if ! "$CC1" -S -g $ARCHFLAGS "$extra" -o "$asmextra" \
                    2> "$OUT/$name.cc1.err"; then
                report no "$name" "cc1 -g refused the second file" \
                    "$(cat "$OUT/$name.cc1.err")"
                continue
            fi
        fi
        if ! gcc "$OUT/$name.s" $asmextra -o "$prog" 2> "$OUT/$name.as.err"; then
            report no "$name" "the assembler refused what cc1 emitted" \
                "$(cat "$OUT/$name.as.err")"
            continue
        fi
    elif ! "$CC1" -g "$src" $extra -o "$prog" 2> "$OUT/$name.cc1.err"; then
        report no "$name" "cc1 -g refused it" "$(cat "$OUT/$name.cc1.err")"
        continue
    fi
    report ok "$name" "compiles with -g"

    stop=$(sed -n 's|^// stop: *||p' "$src" | head -1)
    func=$(sed -n 's|^// func: *||p' "$src" | head -1)
    step=$(sed -n 's|^// step: *||p' "$src" | head -1)
    bt=$(sed -n 's|^// bt: *||p' "$src" | head -1)
    stopin=$(sed -n 's|^// stopin: *||p' "$src" | head -1)

    # Stop where the line says.
    if [ -n "$stop" ]; then
        if [ "$DBG" = lldb ]; then
            log=$(lldb -b -o "breakpoint set --file $base --line $stop" -o run \
                       -o "frame info" -o kill -- "$prog" 2>&1)
        else
            log=$(gdb -batch -ex "break $base:$stop" -ex run -ex "info line" \
                      "$prog" 2>&1)
        fi
        echo "$log" > "$OUT/$name.stop.log"
        if echo "$log" | grep -q -e "at $base:$stop" -e "Line $stop of"; then
            report ok "$name" "stops at $base:$stop"
        else
            report no "$name" "did not stop at $base:$stop" "$log"
        fi
    fi

    # Stop in a file that is not the one holding main. Same question as
    # 'stop:' above, asked of the second compilation unit - which is where a
    # unit pointing at the wrong line program shows and nowhere else.
    if [ -n "$stopin" ]; then
        infile=$(echo "$stopin" | awk '{print $1}')
        inline=$(echo "$stopin" | awk '{print $2}')
        if [ "$DBG" = lldb ]; then
            log=$(lldb -b -o "breakpoint set --file $infile --line $inline" -o run \
                       -o "frame info" -o kill -- "$prog" 2>&1)
        else
            log=$(gdb -batch -ex "break $infile:$inline" -ex run -ex "info line" \
                      "$prog" 2>&1)
        fi
        echo "$log" > "$OUT/$name.stopin.log"
        if echo "$log" | grep -q -e "at $infile:$inline" -e "Line $inline of"; then
            report ok "$name" "stops at $infile:$inline, in the other file"
        else
            report no "$name" "did not stop at $infile:$inline" "$log"
        fi
    fi

    # A function's name lands past its prologue, on a line of its body.
    if [ -n "$func" ]; then
        fname=$(echo "$func" | awk '{print $1}')
        fline=$(echo "$func" | awk '{print $2}')
        if [ "$DBG" = lldb ]; then
            log=$(lldb -b -o "breakpoint set --name $fname" -- "$prog" 2>&1)
        else
            log=$(gdb -batch -ex "break $fname" "$prog" 2>&1)
        fi
        echo "$log" > "$OUT/$name.func.log"
        # gdb names the file by the path it was given and lldb by its base,
        # so the basename is all that is matched on - with the comma, which
        # is what makes it gdb's "..., line 12" and not a line number found
        # loose in the text.
        if echo "$log" | grep -q -e "at $base:$fline" -e "$base, line $fline"; then
            report ok "$name" "'$fname' resolves to $base:$fline"
        else
            report no "$name" "'$fname' did not resolve to $base:$fline" "$log"
        fi
    fi

    # One step arrives where the source says it should.
    if [ -n "$step" ]; then
        from=$(echo "$step" | awk '{print $1}')
        to=$(echo "$step" | awk '{print $2}')
        if [ "$DBG" = lldb ]; then
            log=$(lldb -b -o "breakpoint set --file $base --line $from" -o run \
                       -o "thread step-over" -o "frame info" -o kill -- "$prog" 2>&1)
        else
            log=$(gdb -batch -ex "break $base:$from" -ex run -ex next \
                      -ex "info line" "$prog" 2>&1)
        fi
        echo "$log" > "$OUT/$name.step.log"
        if echo "$log" | grep -q -e "at $base:$to" -e "Line $to of"; then
            report ok "$name" "steps from $from to $to"
        else
            report no "$name" "did not step from $from to $to" "$log"
        fi
    fi

    # Printing an object by name: the whole point of the type information.
    prints=$(sed -n 's|^// print: *||p' "$src")
    if [ -n "$prints" ]; then
        cmds=""
        while read -r expr want; do
            [ -z "$expr" ] && continue
            if [ "$DBG" = lldb ]; then
                cmds="$cmds -o \"print $expr\""
            else
                cmds="$cmds -ex \"print $expr\""
            fi
        done <<PRINTS
$prints
PRINTS
        if [ "$DBG" = lldb ]; then
            log=$(eval lldb -b -o "\"breakpoint set --file $base --line $stop\"" \
                       -o run $cmds -o kill -- "$prog" 2>&1)
        else
            log=$(eval gdb -batch -ex "\"break $base:$stop\"" -ex run $cmds \
                      "$prog" 2>&1)
        fi
        echo "$log" > "$OUT/$name.print.log"
        while read -r expr want; do
            [ -z "$expr" ] && continue
            # gdb answers '$1 = 111' and lldb '(int) 111', so what is common
            # is the value at the end of a line after a space or a bracket.
            if echo "$log" | grep -qE "[ )]$want\$"; then
                report ok "$name" "print $expr is $want"
            else
                report no "$name" "print $expr was not $want" "$log"
            fi
        done <<PRINTS
$prints
PRINTS
    fi

    # A backtrace names the caller as well as the callee.
    if [ -n "$bt" ]; then
        callee=$(echo "$bt" | awk '{print $1}')
        caller=$(echo "$bt" | awk '{print $2}')
        if [ "$DBG" = lldb ]; then
            log=$(lldb -b -o "breakpoint set --name $callee" -o run -o bt \
                       -o kill -- "$prog" 2>&1)
        else
            log=$(gdb -batch -ex "break $callee" -ex run -ex bt "$prog" 2>&1)
        fi
        echo "$log" > "$OUT/$name.bt.log"
        if echo "$log" | grep -q "$callee" && echo "$log" | grep -q "$caller"; then
            report ok "$name" "backtrace names $callee and $caller"
        else
            report no "$name" "backtrace missed $callee or $caller" "$log"
        fi
    fi
done

if [ -z "$ARCH" ]; then
# -g is refused for the spelling that cannot honour it, and says which and
# why. Accepting it and writing nothing would be the bug this prevents.
err=$("$CC1" -g -S -arch x86_64-windows "$SRC/lines.c" -o /dev/null 2>&1)
if [ $? -ne 0 ] && echo "$err" | grep -q "x86_64-windows"; then
    report ok "-g" "is refused for x86_64-windows in MASM, by name"
else
    report no "-g" "was not refused for x86_64-windows in MASM" "$err"
fi

# ...and accepted in the spelling that can, which is the other half. A
# refusal that never lifts is indistinguishable from one that is always right.
if "$CC1" -g -S -arch x86_64-windows -masm=gnu "$SRC/lines.c" \
        -o "$OUT/wgnu.s" 2>/dev/null && grep -q '\.debug_info' "$OUT/wgnu.s"; then
    report ok "-g" "is accepted for x86_64-windows with -masm=gnu"
else
    report no "-g" "wrote no DWARF for x86_64-windows with -masm=gnu"
fi

# And nothing leaks into an ordinary compile.
"$CC1" -S "$SRC/lines.c" -o "$OUT/plain.s" 2>/dev/null
if grep -q -e '\.loc' -e '\.debug' "$OUT/plain.s"; then
    report no "-g" "debug directives appear without -g" \
        "$(grep -n -e '\.loc' -e '\.debug' "$OUT/plain.s" | head -3)"
else
    report ok "-g" "writes nothing without it"
fi
fi

printf 'debug (%s%s)  PASS: %d   FAIL: %d\n' "$DBG" "$WHAT" "$pass" "$fail"
[ "$fail" -eq 0 ]
