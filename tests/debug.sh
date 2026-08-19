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
#   // print: E V      stopped at the 'stop' line, printing E gives V
#
# The print directives of one case are answered in a single run, which is why
# every value a case expects is a different one: the check is that the value
# appears as the answer to something, and two expectations sharing a value
# could cover for each other.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="$ROOT/cc1"
SRC="$ROOT/tests/debug"
OUT="$ROOT/tests/out-debug"

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

    if ! "$CC1" -g "$src" -o "$prog" 2> "$OUT/$name.cc1.err"; then
        report no "$name" "cc1 -g refused it" "$(cat "$OUT/$name.cc1.err")"
        continue
    fi
    report ok "$name" "compiles with -g"

    stop=$(sed -n 's|^// stop: *||p' "$src" | head -1)
    func=$(sed -n 's|^// func: *||p' "$src" | head -1)
    step=$(sed -n 's|^// step: *||p' "$src" | head -1)
    bt=$(sed -n 's|^// bt: *||p' "$src" | head -1)

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

# -g is refused for the target that cannot honour it, and says which target
# and why. Accepting it and writing nothing would be the bug this prevents.
err=$("$CC1" -g -S -arch x86_64-windows "$SRC/lines.c" -o /dev/null 2>&1)
if [ $? -ne 0 ] && echo "$err" | grep -q "x86_64-windows"; then
    report ok "-g" "is refused for x86_64-windows, by name"
else
    report no "-g" "was not refused for x86_64-windows" "$err"
fi

# And nothing leaks into an ordinary compile.
"$CC1" -S "$SRC/lines.c" -o "$OUT/plain.s" 2>/dev/null
if grep -q -e '\.loc' -e '\.debug' "$OUT/plain.s"; then
    report no "-g" "debug directives appear without -g" \
        "$(grep -n -e '\.loc' -e '\.debug' "$OUT/plain.s" | head -3)"
else
    report ok "-g" "writes nothing without it"
fi

printf 'debug (%s)  PASS: %d   FAIL: %d\n' "$DBG" "$pass" "$fail"
[ "$fail" -eq 0 ]
