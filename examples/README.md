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
