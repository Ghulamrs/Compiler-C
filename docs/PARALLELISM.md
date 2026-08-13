# Compiling in parallel

What can be done concurrently in a C compiler, what cannot, and what this one
should do about it. Written because the eventual product is meant to use many
threads rather than compiling one file at a time, and the difference between
the parallelism that pays and the parallelism that cannot work is not obvious.

One piece of this is now implemented: the driver's independent jobs run on
threads, at four or more inputs by default, with `-j 1` to force the serial
loop. Everything below it remains a shape chosen early, while that is cheap.

---

## 1. Where the time actually is

Measured on the development box, over all 361 test programs:

| | |
| --- | --- |
| `cc1`, all 361 files in one invocation | **0.02 s** |
| `cc1`, the same files as 361 separate invocations | 0.61 s |
| `gcc -S` over the same files | 5.05 s |
| The whole differential suite | 28.2 s at one job, 21.3 s at two, 21.3 s at four |
| Largest single program (220 lines) | 1.2 ms, 4 MB |

And a load heavy enough to leave process startup behind — 12 files, 432 013
lines, generated:

| | |
| --- | --- |
| `gcc -O0 -S`, file by file | **24.38 s** |
| `cc1 -j 1` | **2.01 s** |
| `cc1 -j 2` | 1.99 s |
| `cc1 -j 4` | 1.99 s |
| `cc1`, one 36 k-line file | 0.19 s, 38 MB |
| `gcc -O0 -S`, the same file | 1.93 s, 121 MB |

Two findings, and neither was the one being looked for.

**Starting the process costs thirty times the compiling done inside it.** The
same work is 0.02 seconds in one process and 0.61 seconds spread over 361 of
them. Whatever speed this driver had to offer was on the table before threads
entered the argument, and it is collected by handing `cc1` several files at once
rather than invoking it several times.

**The threads buy one per cent here, and that is the machine rather than the
code.** At 432 000 lines — where each file is 0.19 s of real work and startup is
noise — `-j 1` takes 2.01 s and `-j 2` takes 1.99 s. That looks like broken
threading until the machine is measured: this box reports two CPUs and has

```
Core(s) per socket:  1
Thread(s) per core:  2
```

one physical core with two SMT siblings. Two pure integer loops take 1.57 s
against 0.77 s for one, so the hardware ceiling for ALU-bound work is about two
per cent — and threads and separate processes both collect exactly that. The
loop is doing what it should on a machine that cannot do two things at once.

That measurement is why `Driver` counts **cores** rather than asking
`std::thread::hardware_concurrency()`, which reports logical CPUs. Believing the
logical count spawns a second thread for one per cent and doubles peak memory
from 45 MB to 83 MB, which on a 419 MiB box is a real price for nothing.

This compiler is eight times faster than gcc on the same input, and that is not
a compliment: it is faster because it does almost nothing. There is no
optimiser, no register allocator, no dataflow analysis. **Any measurement of
parallelism taken today measures process startup** — which, together with the
SMT finding above, is why the threads are worth having for the shape rather than
for the time.

The numbers change entirely once there is an optimiser. In a production
compiler the front end is a small fraction and the middle and back ends
dominate, and that is exactly where the parallelism lives.

### Where the time goes inside one compilation

`cc1 -time` reports it. On generated programs of increasing size, after the
fix described below:

| functions | lines | read+pp | lex | parse | codegen | total | front end |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 000 | 7 k | 3.8 | 11.6 | 9.6 | 3.5 | 28.5 | 88% |
| 2 000 | 14 k | 7.5 | 23.4 | 19.0 | 7.2 | 57.0 | 87% |
| 4 000 | 28 k | 15.1 | 47.7 | 37.7 | 14.0 | 114.6 | 88% |
| 8 000 | 56 k | 30.1 | 95.8 | 76.2 | 28.1 | 230.2 | 88% |
| 8 000 (larger bodies) | 96 k | 51.4 | 182.5 | 155.9 | 67.1 | 456.8 | 85% |

Milliseconds. The preprocessor is its own column now and was not when this was
first measured; it costs a steady 13 per cent. Doubling the input doubles the
total — 28.5, 57.0, 114.6, 230.2 — which is the quadratic parse described below
staying fixed.

**The front end dominates, at 80 to 88 per cent**, and the lexer is now the
single largest phase. So for *this* compiler, work grows on the front end and
the back end is a minority of it.

That is not a general truth about compilers, and it is worth seeing why. gcc on
the same 14 000-line file, by its own `-ftime-report`:

| | parsing | opt and generate |
| --- | --- | --- |
| `gcc -O0` | 13% | **87%** |
| `gcc -O2` | 6% | **94%** |

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

**Done, and the prediction held with one exception worth recording.** The loop
itself was contained: a shared index, a thread per core, and `compile` unchanged
because everything it needs was already local to it. What it did reach into was
diagnostics. `Source::fail` and `Preprocessor::fail` each printed a message in
three writes — the header, the offending line, then the caret — and two threads
failing at once interleaved them into a message belonging to neither. Both now
compose the whole thing and write it once. That is two functions in the
compiler, not none, and the general form of the lesson is that *shared output is
shared state* even when no data structure is.

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
| Threads over driver jobs | **done** | measured: one per cent, which is this machine's SMT ceiling and not the loop's fault |
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
and four jobs, and that was checked rather than assumed — as is the compiler's
own output, by a suite case that compiles the whole corpus twice, `-j 1` against
`-j 4`, and requires the assembly to match byte for byte.
