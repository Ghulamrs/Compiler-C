# examples

Ten programs, one per area of the language, and a caller that joins them.

```
../cc1 *.c            compile every file in one invocation
gcc *.s -o examples   assemble and link
./examples
```

Each file is a translation unit that knows nothing of its neighbours until the
linker joins them, which is the point: this is the shape a real C program has,
and compiling it exercises the driver's multi-file path rather than the
single-file one the test suite mostly uses.

| | |
| --- | --- |
| `ex01_arithmetic.c` | the integer types, promotions, and signedness choosing the instruction |
| `ex02_control.c` | if, the three loops, switch with fallthrough, break, continue, goto |
| `ex03_pointers.c` | addresses, arithmetic that scales, decay, a matrix from `malloc` |
| `ex04_structs.c` | struct, union, enum, typedef, padding, a self-referencing list |
| `ex05_bitfields.c` | packing, signed fields, `: 0`, and read-modify-write |
| `ex06_strings.c` | `<string.h>` over arrays that decay at the call |
| `ex07_floats.c` | the SSE register file, two argument lanes, Newton's method |
| `ex08_fnptr.c` | pointers to functions, a dispatch table, and libc's `qsort` |
| `ex09_initialisers.c` | counted lengths, short lists, strings into arrays, nesting |
| `ex10_fileio.c` | `FILE` through an incomplete type, text and binary |
| `heavy.c` | a long program, for compile time rather than run time |
| `main.c` | calls all ten and checks the total |

`heavy.c` is here to be compiled rather than admired: it is deliberately long
and deliberately quick to run, because what it measures is the front end. The
suite reaches this whole directory through `tests/multi/examples`, so every
example is compiled, linked, run, and compared against gcc's build of the same
sources on every `./build test`.

---

## `CC1Lab/` — the same program on two targets

An Xcode project that builds with cc1 in clang's place, and the one place a
program written once is put through two backends and two C libraries to see
whether it says the same thing.

It is a non-local jump across two translation units: `env` is defined in
`risky.c` and `extern` in `risky.h`, so the `longjmp` crosses a boundary the
compiler cannot have arranged for. `main.c` uses `int status = setjmp(env)` —
the initialiser form, which is the spelling that used to store its result
through a wild pointer.

**On arm64-darwin**, through Xcode:

```
xcodebuild -project CC1Lab.xcodeproj -scheme CC1Lab \
  ARCHS=arm64 ONLY_ACTIVE_ARCH=YES build
```

`ARCHS` is not optional — Xcode builds x86_64 as well by default and there is
no backend for it. The `CC` setting is already in the project, so building from
the IDE needs no argument at all.

**On x86_64-windows**, over ssh:

```
./CC1Lab/on-windows.sh
```

which compiles the same two files for the other target, relays the MASM to the
Windows host, and has `ml64` and `link.exe` make a binary there. Nothing in the
lab is written twice for it: `<setjmp.h>` is the whole of what differs, and all
of that is inside the shipped header and the compiler.

The two runs differ in exactly one line, and are meant to:

```
jmp_buf: 192 bytes, 16-byte aligned=1        arm64-darwin
jmp_buf: 256 bytes, 16-byte aligned=1        x86_64-windows
```

`setjmp` lives in the C library, so the buffer is as large as that library was
built to fill. The alignment is the interesting half. The UCRT fills a
`jmp_buf` with aligned `xmm` saves, so `recover_locally` in `risky.c` — which
puts its buffer in a frame rather than at file scope — is the case that fails
on a compiler that cannot align a local past eight bytes. Taking the
sixteen-byte rule out of `objectAlign` turns that line into an access
violation, which is how it was checked rather than assumed.
