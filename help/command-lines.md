# Command lines, per architecture

What to type to get a `.s`, an object, or a program, for each of the three
targets — and which of those three each target can actually reach from the
machine you are sitting at.

Every claim here was produced by running the command, not by reading the code.

---

## The three architectures

```
x86_64-linux    x86_64-windows    arm64-darwin
```

`-arch=NAME` and `-arch NAME` are the same thing. With no `-arch` at all you
get the host, whatever that is.

`cc1` with no arguments prints the same list and the flags below.

---

## The rule that governs all of it

**cc1 has no assembler and no linker of its own.** It writes assembly, and then
calls the host's `cc` to turn that into an object or a program. So:

- the **native** target reaches `.s`, `.o` and an executable;
- **any other** target reaches `.s` and stops there, and says so:

```
cc1: cannot assemble x86_64-windows code on this machine, which is
     arm64-darwin - use -S to write the assembly and take it there
```

This is why Windows is a two-machine job from a Mac or from Linux.

**cc1 can be built on Windows now**, which removes the second machine: see
[`../msvc/readme.txt`](../msvc/readme.txt). That build compiles and writes
assembly like any other, and `msvc/cc1-as-cl.bat` hands the assembling to
`ml64` so Visual Studio can use cc1 as its C compiler. What it does *not* do is
assemble or link from `cc1` itself — the driver builds those command lines for
a POSIX shell, and `cmd.exe` understands neither the quoting nor the flags — so
`-S` is the mode there and one more command finishes the job.

---

## x86_64-linux

Native on the EC2 box. Everything works in one command.

Assembly only:

```bash
cc1 abc.c -arch=x86_64-linux -S -o abc.s
```

Object file:

```bash
cc1 abc.c -arch=x86_64-linux -c -o abc.o
```

Executable:

```bash
cc1 abc.c -arch=x86_64-linux -o abc
```

Several files link together, and `-o` names the program:

```bash
cc1 main.c helper.c -o app
```

From the Mac, only the first of these works — the rest are cross-compiles.

---

## arm64-darwin

Native on the Mac. Everything works in one command.

Assembly only:

```bash
cc1 abc.c -arch=arm64-darwin -S -o abc.s
```

Object file (Mach-O arm64):

```bash
cc1 abc.c -arch=arm64-darwin -c -o abc.o
```

Executable:

```bash
cc1 abc.c -arch=arm64-darwin -o abc
```

Inside Xcode this is reached a different way: the `CC1Lab` project sets `CC` to
`tools/cc1-as-clang`, which translates Xcode's sixty-flag command line into
this one. See `examples/README.md`.

---

## x86_64-windows

**cc1 writes MASM here by default**, because `ml64` is the assembler this
platform ships and `link.exe` is its linker. Nothing on a Mac or a Linux box
can assemble it, so the command stops at `-S` and the rest happens on Windows.

### Step 1, wherever cc1 runs

```bash
cc1 abc.c -arch=x86_64-windows -S -o abc.asm
```

`-c` and the executable form are **refused** from a non-Windows host. That is
the cross-compile rule above, not a gap in the backend.

### Step 2, on the Windows machine

```
ml64.exe /nologo /c /Fo abc.obj abc.asm
```

### Step 3, on the Windows machine

```
link.exe /nologo /subsystem:console /out:abc.exe abc.obj libcmt.lib libucrt.lib libvcruntime.lib kernel32.lib legacy_stdio_definitions.lib
```

Both steps need `vcvars64.bat` to have been run first — that is what puts
`ml64` and `link` on `PATH` and sets `LIB`.

**Why those five libraries.** `link.exe` driven directly is told nothing, where
a compiler driver would have embedded `-defaultlib` directives in the object.
`libcmt` brings in `mainCRTStartup`, the entry point that calls `main`.
`legacy_stdio_definitions` is not optional for anything that formats into a
buffer: Microsoft's UCRT kept `printf`, `fprintf`, `puts` and `fputs` as real
exported symbols but made `sprintf`, the whole v-family and the scanf family
inline wrappers over `__stdio_common_*` in its own `<stdio.h>`. A compiler that
declares them as the ordinary functions C says they are — which cc1 does,
correctly — has nothing to link against without it.

`tests/masm-native.sh` does all three steps over the whole Windows corpus, and
[`../examples/CC1Lab/on-windows.sh`](../examples/CC1Lab/on-windows.sh) does
them for one program — the lab that Xcode builds for arm64, compiled for this
target instead and run on the Windows machine, which is the shortest way to see
the whole path work.

**The objects carry unwind data.** Each function is written as a `PROC FRAME`
with its prologue described — `.PUSHREG`, `.SETFRAME`, `.ALLOCSTACK`,
`.ENDPROLOG` — and `ml64` builds the `.pdata` and `.xdata` sections from that.
This is not an optional nicety on x64: the platform has no frame-pointer walk
to fall back on, so a function with no entry is a wall that `RtlUnwindEx` stops
at and a debugger cannot see past. `dumpbin /unwindinfo` on a linked binary
decodes it.

### The other syntax

```bash
cc1 abc.c -arch=x86_64-windows -masm=gnu -S -o abc.s
```

writes the GNU spelling instead, which gcc and clang read. That is how
`tests/windows.sh` cross-assembles Windows code on Linux, and how
`tests/windows-native.sh` hands it to clang on Windows. `-masm=masm` is the
default and never needs typing.

**This spelling carries no unwind data**, and cannot: GAS has no `.seh_*`
directives when it is built for ELF — it rejects them outright — and the suite
above assembles this output with gcc on Linux. Nothing in the corpus needs
them, since `longjmp` here is told to restore rather than unwind; a Windows
binary built through clang rather than `ml64` would.

---

## What each target can compile

Measured by putting all 412 single-file cases through each backend and counting
what came out. This is coverage of the *language*, and is a different question
from which stages of the pipeline are reachable.

| Target | Compiles | Refuses | What it still refuses |
| --- | --- | --- | --- |
| `x86_64-linux` | **412 / 412** | 0 | nothing |
| `x86_64-windows` | **411 / 412** | 1 | nothing that is a gap. The one refusal, `bf_types.c`, asks for a 40-bit field in an `unsigned long` — 32 bits under LLP64 — so refusing is correct C. |
| `arm64-darwin` | **412 / 412** | 0 | nothing |

**All three targets compile everything in the corpus** that is correct C and
available for them, and the one refusal left is one Windows is right to make.

Note that this table is produced with `-S`, and stopping there is not proof the
output assembles. `fcvt d0, d0` — arm64's double-to-`long double`, which is a
conversion in the type system and nothing at all in the machine — was written
by a backend that counted as compiling in every column here, and the assembler
rejected it. The differential runs below are what settle it.

arm64 got there in four steps, having stood at eight refusals: the variadic
part, calls through a function pointer, arguments past the eighth register, and
finally aggregates that do not fit in the registers left for them.

**Apple's stack argument layout is not the one AAPCS64 describes**, and it takes
three rules rather than one:

| | alignment | size |
| --- | --- | --- |
| a named scalar | its own | its own — four `int`s take 16 bytes, not 32 |
| a named aggregate | at least 8 | rounded up to a multiple of 8 |
| anything variadic | 8 | 8 |

A 12-byte struct placed after a `char` therefore starts at 8, not 4, and
occupies 16. All three were read off clang rather than assumed. A single call
can use all three at once.

An aggregate also goes **wholly** in registers or **wholly** in memory, never
split — and one that goes to memory closes its own register file to every later
argument while leaving the other open.

This is the one place where being wrong is invisible from the inside: a caller
and a callee that are wrong in the same way agree with each other perfectly. So
both walks call one function, and the check that counts is a cross-toolchain
link — cc1's caller against clang's callee and the reverse. That check has
already earned itself once, catching a prologue that destroyed a register
parameter while reading a stack one.

`int (*get(void))(void)` — a function *returning* a function pointer — compiles
now too, on every target. It was never a backend gap: the declarator is the
whole of the difficulty, and a returned function pointer is an address like any
other.

Aggregates crossing a function boundary work on all three now, and each does it
its own way. System V cuts one into eightbytes and classifies each. Microsoft
x64 puts it in a register only at sizes 1, 2, 4 and 8, and copies anything else
for a pointer. AAPCS64 has a third rule again: one to four members of the same
floating type — an HFA — go in that many vector registers whatever the size, so
three doubles travel in d0-d2; anything else of 16 bytes or less goes in one or
two integer registers, and larger is copied for a pointer, with x8 carrying the
address of a returned one.

arm64 was at 352 until member selection learned to compute an address. That one
gap was 31 of its 44 refusals, because `&` is not the only thing needing an
address: reading `s.n` needs one, and so does every bit-field, every `->` and
every whole-struct assignment.

All 412 agree with clang exactly, checked by compiling each twice and comparing
what the two programs print and return.

`long double` is the one type where the three targets disagree about what they
are being asked for. System V gives it x87's 80-bit extended format in sixteen
bytes; Apple's arm64 and the UCRT both make it another spelling of `double`. So
the same source compiles everywhere and `1.0L/3.0L` prints four more correct
digits on Linux than on the other two — which C90 allows, asking only that the
type be no narrower than `double`.

The refusals that remain in that backend name themselves and the target, so
nothing fails silently — none of them is about the calling convention any more,
and none is reached by the corpus:

```
codegen: this binary operator is not supported yet by the arm64-darwin backend
```

---

## All the flags

| Flag | Does |
| --- | --- |
| *(neither `-S` nor `-c`)* | compile, assemble and link into a program, named by `-o` or `a.out` |
| `-c` | stop at one object per input |
| `-S` | stop at assembly, one `.s` per input |
| `-o NAME` | name the output |
| `-arch NAME` | pick the architecture; the host by default |
| `-masm=masm\|gnu` | assembly syntax for `x86_64-windows`; `masm` by default |
| `-D n[=v]`, `-U n` | define and undefine macros, including the target's own |
| `-I dir` | add a directory to the `<...>` search path |
| `-j n` | how many files compile at once; `-j 1` is serial |
| `-time` | report how long each phase took |

`CC1_CC` names the host compiler cc1 shells out to for assembling and linking,
where the default is `cc`.
