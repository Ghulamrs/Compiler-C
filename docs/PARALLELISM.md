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

### Where the time goes inside one compilation

`cc1 -time` reports it. On generated programs of increasing size, after the
fix described below:

| functions | lines | lex | parse | codegen | front end |
| --- | --- | --- | --- | --- | --- |
| 1 000 | 3 k | 5.1 | 3.0 | 1.2 | 87% |
| 2 000 | 6 k | 9.8 | 6.1 | 2.4 | 87% |
| 4 000 | 12 k | 20.1 | 12.4 | 4.8 | 87% |
| 8 000 | 24 k | 41.4 | 26.2 | 9.5 | 88% |
| 8 000 (larger bodies) | 64 k | 185.1 | 125.1 | 80.5 | 80% |

**The front end dominates, at 80 to 88 per cent**, and the lexer is now the
single largest phase. So for *this* compiler, work grows on the front end and
the back end is a minority of it.

That is not a general truth about compilers, and it is worth seeing why. gcc on
the same 16 000-line file, by its own `-ftime-report`:

| | parsing | opt and generate |
| --- | --- | --- |
| `gcc -O0` | 21% | **79%** |
| `gcc -O2` | 7% | **93%** |

The difference is the optimiser. This compiler's back end is small because it
does almost nothing: no optimisation, no register allocation, no dataflow. A
production compiler spends its time where the passes are, and the passes are
per function - which is exactly the parallelism section 3 describes. Both
statements are true at once: the front end dominates here, and it will not
dominate once there is a middle end worth the name.

### The finding that came out of measuring

Timing the phases turned up something better than a scheduling opportunity.
Parsing was **quadratic in the number of functions**:

| functions | parse, before | parse, after |
| --- | --- | --- |
| 1 000 | 5.65 ms | 2.96 ms |
| 2 000 | 12.99 ms | 6.05 ms |
| 4 000 | 48.33 ms | 12.37 ms |
| 8 000 | **200.44 ms** | **26.18 ms** |

Every call and every declaration walked the whole function table, which was a
vector searched linearly. Lexing and code generation over the same files were
linear, which is what made the parser the obvious suspect. The tables are now
indexed by name; parse doubles with the input, as it should.

**Seven and a half times, from a hash map.** No arrangement of two threads
could have come close, and that is the general lesson: on a program this
compiler is slow on, the first question is what is quadratic, not what could
run concurrently.

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
3. **Emission must not interleave.** **Now true.** Each function is emitted
   into its own buffer and the buffers are concatenated in source order, so no
   two functions ever write to the same stream.

### The change that was worth making early — done

`CodeGen` emits each function into its own `ostringstream` and concatenates in
source order. The output order no longer depends on the emission order, which
is worth having on its own, and "parallelise the back end" is now a change to
one loop rather than to every place that writes an instruction.

The label counter went per-function at the same time, and that needed labels to
carry the name of the function that owns them: `.L.is_even.end.0` rather than
`.L.end.0`. A single shared counter would have been the one piece of state two
threads could not both hold, and per-function counters without the name would
have collided in the assembler.

It cost 7 MB on the compile of `CodeGen.cpp` — 100 to 107 — for `<sstream>`.

---

## 4. What this compiler should do, and when

| | When | Why |
| --- | --- | --- |
| Independent driver jobs | **done** | costs nothing, and it is the unit that matters |
| Per-function emission buffers | **done** | small now, a redesign later |
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
