# Compiling in parallel

What can be done concurrently in a C compiler, what cannot, and what this one
should do about it. Written because the eventual product is meant to use many
threads rather than compiling one file at a time, and the difference between
the parallelism that pays and the parallelism that cannot work is not obvious.

Nothing here is implemented beyond the driver's independent jobs. The point is
that the shape is chosen now, while it is cheap.

---

## 1. Where the time actually is

Measured on the development box, over all 191 test programs:

| | |
| --- | --- |
| `cc1` compiling all 191 files | **0.31 s** |
| `gcc -S` compiling the same files | 2.59 s |
| The whole differential suite | 13.9 s serial, 10.4 s on two cores |
| Largest single program (220 lines) | below timer resolution, 4 MB |

This compiler is eight times faster than gcc on the same input, and that is not
a compliment: it is faster because it does almost nothing. There is no
optimiser, no register allocator, no dataflow analysis. **Any measurement of
parallelism taken today measures process startup**, which is why the driver's
jobs are independent but not threaded — 0.304 s serial against 0.221 s on two
processes, and 0.219 s on four when the machine has two cores.

The numbers change entirely once there is an optimiser. In a production
compiler the front end is a small fraction and the middle and back ends
dominate, and that is exactly where the parallelism lives.

---

## 2. What cannot be parallelised, and why

### Parsing one file

C cannot be parsed without the symbol table built by everything before the
current token:

```c
typedef unsigned char Byte;
...500 lines...
x = (Byte)*p;        /* a cast */
x = (count)*p;       /* a multiplication */
```

The two lines are the same shape. Only the declarations above decide which is
which. So a scheme that splits the token stream into chunks and parses them
concurrently would first have to parse the declarations to learn where the
chunks may safely fall — the sequential work it set out to avoid.

This is not a performance objection. **Splitting is incorrect**, not merely
unprofitable. Any C compiler that claims parallel parsing of one file is either
speculating and re-running on conflict, or is not parsing C.

The same is true, more sharply, of the preprocessor: `#include` and `#if` mean
the token stream does not exist until it has been produced in order.

### Pipeline parallelism between stages

Running the lexer in one thread feeding the parser in another sounds appealing
and is not worth it. The handoff is fine-grained — one token at a time — so
synchronisation costs more than the work, and the parser stalls on the symbol
table anyway. Compilers that tried this abandoned it.

---

## 3. What can be parallelised

### Across translation units — already structured for

The unit of parallelism in every real C toolchain. Each file is compiled by a
separate process that shares nothing, and `make -j` schedules them. This
compiler's `Driver` already models a job that way: its own `Source`,
`TypeTable`, `Parser` and `CodeGen`, no shared state, one output file. Turning
the loop into threads is a small change **in the driver**, touching no part of
the compiler.

Cheap, correct, and the first thing to reach for.

### Across functions, once a file is parsed — the real answer

After parsing, the file is a list of functions and a symbol table that no
longer changes. From that point each function is independent:

```
     lex ─► parse ─► symbol table          SEQUENTIAL, and must be
                          │
              ┌───────────┼───────────┐
              ▼           ▼           ▼
          check f1    check f2    check f3     PARALLEL
          optimise    optimise    optimise     PARALLEL  ← where the time is
          codegen     codegen     codegen      PARALLEL
              └───────────┼───────────┘
                          ▼
                  emit in source order        SEQUENTIAL, and cheap
```

This is how LLVM works: per-function passes over a module, and ThinLTO
parallelises across modules on the same principle. It is the design worth
building towards, because optimisation is where a real compiler spends its
time and optimisation is per-function.

Three things have to be true for it, and two of them already are:

1. **Functions must not share mutable state.** True today — each `Function`
   owns its body and frame size.
2. **The type table must be read-only after parsing.** True today. `pointerTo`,
   `arrayOf` and `structType` intern new types during parsing and are never
   called after it. If that ever changes, the table needs a lock or a
   per-thread arena merged at the end.
3. **Emission must not interleave.** *Not true today* — `CodeGen` writes
   straight to one `std::ostream`, so two functions emitting at once would
   shuffle their instructions together.

### The one change worth making early

Point 3 is the only structural obstacle, and it is small: have `CodeGen` emit
each function into its own buffer, and concatenate the buffers in source order
at the end. That is worth doing on its own merits — it makes the output order
independent of the emission order — and it converts "parallelise the back end"
from a redesign into a scheduling change.

The label counter must become per-function at the same time, or two threads
will hand out the same `.L.end.3`. It is already used per-function in practice.

---

## 4. What this compiler should do, and when

| | When | Why |
| --- | --- | --- |
| Independent driver jobs | **done** | costs nothing, and it is the unit that matters |
| Per-function emission buffers | before any optimiser | small now, a redesign later |
| Threads over driver jobs | when a build has many files and they are slow | measured: worth 80 ms today |
| Threads over functions | **when there is an optimiser** | nothing to schedule until then |

The order matters. Threading the back end before there is a back end worth
threading would add locks, non-determinism and a debugging burden to a stage
that currently takes microseconds — and it would have to be redone anyway once
the passes it was meant to parallelise are written.

---

## 5. Determinism is not negotiable

Whatever is threaded, the output must not depend on the schedule. Two
compilations of the same source must produce byte-identical assembly, or
`demo/refresh.sh` cannot check that a recorded artifact is current, reproducible
builds become impossible, and a bug that appears once in twenty runs is a bug
nobody can bisect.

That is why emission is ordered by source position rather than by completion,
and why the test suite — which *is* parallel now — collects each case's verdict
separately and prints them in name order. Its output is identical at one, two
and four jobs, and that was checked rather than assumed.
