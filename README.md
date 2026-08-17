# ansicc

An ANSI C compiler, written by hand in C++, for three targets: x86-64 System V,
Microsoft x64, and Apple's arm64. All three emit, and all three run what they
emit.

## Where it runs

Development is on the AWS box, not on the Mac. The Mac is the control room: a
terminal, a monitor, and nothing else. Everything else — compiler, assembler,
linker, debugger, test corpus — lives on the remote machine.

The reason is concrete. The Mac has no GNU compiler (`/usr/bin/g++` is Apple
clang wearing a GCC mask: it defines `__clang__` *and* `__GNUC__ 4`), ships
bison 2.3 from 2006, is arm64 so every reference implementation's x86-64 output
needs Rosetta, and follows Apple's non-standard arm64 variadic rule where
`printf` arguments go on the stack rather than in registers. The Linux box has
real GCC, bison 3.7.4, native x86-64 and the standard SysV ABI — the exact
environment every C compiler reference assumes.

## Building

```
./build            build
./build test       build and run the differential suite
./build clean
```

[`msvc/`](msvc) builds it with MSVC instead, so the compiler runs on the
target it generates for — and lets Visual Studio compile C with cc1 in
`cl.exe`'s place. [`msvc/readme.txt`](msvc/readme.txt) is the procedure.
Nothing in `src/` is restructured for it: the one dependency outside C++17 is
`getpid`.

`cc1 -arch <name>` picks the architecture code is generated for, **defaulting to
the host it was built on** — the Mac build targets `arm64-darwin`, the Linux
build `x86_64-linux`. All three emit, and each compiles every case in the
corpus that is correct C for it — the only refusal anywhere is a 40-bit
bit-field on Windows, where `unsigned long` is 32 bits and refusing is right.
`tests/arm64.sh` checks `arm64-darwin` against clang and must be run on the
Mac, since the Mac is the machine that can execute it. `x86_64-windows` writes
**MASM** by default, which `ml64` assembles and `link.exe` links, so nothing in
that path is borrowed from another toolchain; `-masm=gnu` writes the GNU
spelling instead, which is what the suites that assemble with gcc and clang
pass.

Use `./build` rather than calling `make` directly. It puts the whole build
inside a memory cgroup, so a compile that runs away is killed by itself instead
of taking the machine down with it. That is not hypothetical: this box has 419
MiB of RAM, and an unbounded `dnf` drove it into swap on 12 August until sshd
could no longer fork, which took a hypervisor power cycle to undo.

The same 419 MiB is why `make` is serial by design — `Parser.cpp` alone costs
142 MB to compile, and `-j2` asks for twice that against roughly 260 MB
available. It is also why the shared headers are kept thin: `<iosfwd>` over
`<ostream>`, and both `<unordered_map>` and `<sstream>` were weighed before
being added. C++ was chosen anyway, for the type system's sake, but it is paid
for in a currency this machine is short of.

## The shape

Four stages, one direction, no passes over the same data twice:

| File | Does |
| --- | --- |
| `src/Preprocessor.cpp` | file → one translation unit: includes, conditionals, macros |
| `lib/*.h` | the library it ships, which is not the language: all fifteen headers C90 defines, from `assert.h` to `time.h` |
| `src/Lexer.cpp` | source text → tokens |
| `src/Parser.cpp` | tokens → tree, recursive descent — **and** type checking, which C cannot separate from parsing, and the constant folder that four parts of the grammar need |
| `src/backend/` | one file per platform, all three emitting: `X86_64Linux` serves System V and Microsoft x64 from one generator, `Masm` respells its output for `ml64`, `Arm64Darwin` is AAPCS64. `Backend.cpp` holds what they share — the registry `-arch` searches, and which of the four segments an object belongs in |
| `src/Type.cpp` | the type model, interning, and the `Target` that owns every size |
| `src/Ast.h` | the node hierarchy and the visitor |
| `src/Source.cpp` | the text, and every diagnostic |
| `src/Driver.cpp` | one job per input file, on threads at four or more — asking the machine how many cores it has; `main.cpp` is nothing but a way in |

10,668 lines of C++ in 24 files, under `-Wall -Wextra -Werror -pedantic
-pthread`, plus 1,060 lines of C in the fifteen headers it ships.

1,346 of those lines are comments, and that ratio moved on purpose. This file
once said nineteen, back when the reasoning lived in commit messages alone.
What is written beside the code now is the part a reader cannot re-derive: why
GNU as reverses `fsub` against the Intel sense, why an assignment takes its
address after its value, why an aggregate holding an x87 `long double` is
MEMORY whatever its size. `git blame` is still the way to ask why a line reads
as it does; the comments are for where the right code and the wrong code look
alike.

Assembling and linking are left to the host's `cc` — `gcc` on Linux, `clang` on
the Mac, and `ml64` with `link.exe` on Windows. That keeps the surface under
test to the part actually being written, and it is what makes the differential
suite below possible.

## Testing

`tests/run.sh` compiles every case **twice**, once with `cc1` and once with
`gcc`, runs both under a five-second limit, and requires that the two agree on
the exit status *and* on what they printed, and that both match the expectation
written at the top of the case.

A case under `tests/multi/` is a directory: its sources are compiled one unit at
a time and linked, which is the only way to test what C is arranged around — a
translation unit knows nothing of its neighbours until the linker joins them.
`demo/multifile` is one of them, so the program its README describes is run
rather than only read.

`tests/challenge.sh` is not a test but a stopwatch: it compiles the corpus
repeatedly under `cc1 -j 1`, `cc1 -j 4` and `gcc -O0 -S`, one invocation each so
the comparison is like for like, and requires every one of those runs to produce
the same assembly. It takes the **minimum** of the rounds rather than the mean,
because on a shared box the minimum is the closest thing to the machine's real
capability.

Two corpora, because they answer different questions. Re-measured at `6d8cbfd`:

| | files | lines | `cc1 -j 1` | `gcc -O0 -S` | | `-j 4` |
| --- | --- | --- | --- | --- | --- | --- |
| the test corpus, 100 rounds | 412 | 5,121 | 0.074s | 5.434s | **73.4x** | **+8.8%** |
| generated, 5 rounds | 12 | 432,013 | 2.209s | 23.490s | **10.6x** | **−5.1%** |

The two speedups differ by seven times over, and the gap is process start rather
than either compiler: 412 files means 412 things for gcc to open, parse and
close, and one `cc1` invocation that does all of them. The 432 000-line corpus
is twelve files, so almost nothing of what it measures is startup — which makes
**10.6x the honest number for throughput** and 73.4x the honest number for a
build of many small files.

**The `-j 4` column is not a measure of the loop, and it took counting the
machine's cores to see it.** This box reports two CPUs, and they are one
physical core with two hyperthreads — `cpu cores: 1, siblings: 2`. So `cc1 -j 4`
is four threads over one core, and what that column measures is contention.
Asking for the thread count the machine actually has says something different,
over the 432 000-line corpus:

| | min | against `-j 1` |
| --- | --- | --- |
| `cc1 -j 1` | 2.285s | |
| `cc1 -j 2` | **2.174s** | **+5.1% faster** |
| `cc1 -j 4` | 2.317s | −1.4% slower |

(Best of five, timed from the shell rather than by `challenge.sh`, which is why
`-j 1` reads 2.285s here and 2.209s above — a different harness with its own
overhead. The three rows are comparable with each other, which is all this table
is for.)

So the job loop does scale on the large corpus, by about what one extra
hyperthread is worth, and the negative figure in the table above is the cost of
asking for four threads on one core rather than a fault in the loop. The earlier
reading of that number as "this machine's SMT ceiling" was the right instinct
attached to the wrong measurement: nothing had counted the cores.

**The driver had already counted them.** `availableCores()` does not ask
`hardware_concurrency()` — that answers 2 here and would be wrong. It reads
`/sys/devices/system/cpu/*/topology` and counts distinct
`(physical_package_id, core_id)` pairs, which is **1** on this box, and it
respects `sched_getaffinity` so a cgroup-restricted build gets the smaller
answer. `cc1 -time -S tests/cases/*.c` reports `412 jobs on 1 thread`: given no
`-j`, the compiler declines to thread here at all, which is the right call on
one core and the reason the `-j 4` column has never described a build anyone
would actually run. It is a stress figure, and worth keeping as one so long as
it is labelled.

The small corpus stays the noisier of the two — its `-j 4` figure has read
anywhere from +8.8% to +19.1% across runs, and 5 121 lines is too little work to
divide, which `challenge.sh` now says in its own output.

Determinism is asserted 200 times over the small corpus and 10 over the large,
against a hash of the whole corpus's assembly rather than one file — which is
what catches something that goes wrong one run in fifty rather than every run.
Both fingerprints were single-valued.

Comparing against gcc rather than against expectations alone is the point: an
expectation is an opinion about C, while gcc is the reference implementation
sitting on the same disk. Where they disagree, the case is wrong until the
standard says otherwise. That has already caught four wrong expectations of
mine rather than compiler bugs.

**But two compilers can agree and both be wrong**, and that is what the two
newest suites are for.

`tests/cross-abi.sh` links cc1's objects against the host compiler's, in both
directions. Everything above compiles a case twice and compares what the two
*separately built* programs print, so a compiler wrong about the ABI in the
same way on both sides of a call agrees with itself and passes. A four-byte
struct came back in `%rdx` where both conventions say `%rax`, the caller read
`%rdx` too, and 421 cases passed over it for as long as it existed. This is the
only check here that puts two compilers' output into one program.

`msvc/run-corpus.ps1` runs all 412 cases natively on Windows with `cl` as the
reference — a question `tests/windows.sh` cannot ask, since it takes only the
18 that survive running under a foreign convention on Linux. It has found five
bugs — three read off its output the first day, and two more sitting unread in
that same run, inside counts with no names against them. Two needed a target
whose `long` is narrower than the host's, one a host that spells its paths with
a different separator, and one a real linker to object; none of which a machine
this is developed on provides. Every case cc1 compiles and `cl` can build now
agrees with `cl`, bar one excluded by name and printed with its reason, whose C
is undefined where `unsigned long` is 32 bits.

**421 cases, all passing** — 412 single files, 8 directories, and one check on the
driver's threaded job loop. Beside them: 18 cases for `x86_64-windows`, run
twice, through clang and through `ml64` on Windows itself, 16 for
`arm64-darwin`, and 8 cross-ABI. They run in parallel, because they are
independent and because the work is not this compiler — `cc1` accounts for
about 0.3s of the 12s a full run takes, and the rest is gcc assembling, gcc
building the reference, and running two binaries per case. Output is collected
per case and printed in name order, so a parallel run reads exactly like a
serial one.

## Examples

[`examples/`](examples) is ten programs, one per area of the language, and a
caller that joins them:

```
cd examples && ../cc1 *.c && gcc *.s -o examples && ./examples
```

Twelve translation units, 1,853 lines, compiled in one invocation — which
exercises the driver's multi-file path rather than the single-file one the suite
mostly uses. `heavy.c` is there to be compiled rather than admired: 1,303 lines
that run in microseconds, because what it weighs is the front end. The whole
directory is reached by the suite through `tests/multi/examples`, so all of it
is compiled, linked, run and compared against gcc on every `./build test`.

## Accepted today

Functions with prototypes and typed parameters, any number of them, with
recursion and mutual recursion. A prototype may name only types, `int printf(char *, ...)`, as a header does; a
definition may not, since a body cannot use what it cannot name. A prototype
must come first — an undeclared
name is refused rather than assumed to return `int`, and every call is checked
against its signature: the number of arguments, the return type, and the type of
each argument against C's constraints on simple assignment.

The integer type system: `char`, `signed char` and `unsigned char` as three
distinct types, `short`, `int`, `long`, `long long`, each signed or unsigned,
with `sizeof`, casts, the integer promotions and the usual arithmetic
conversions. Signedness selects the instruction and not merely the type, so
`-1 >> 1` stays `-1` where the unsigned shift gives 2147483647.

`float` and `double`, in their own register file, with the System V rule that
integer and floating arguments are counted in separate lanes. Variadic
prototypes, so `printf("%d %.2f\n", n, x)` works.

Pointers, arrays, string literals and globals: `&x`, `*p`, `a[i]`, pointer
arithmetic that scales by the element, arrays that decay to pointers when used
as values but not under `sizeof`, and `static` for internal linkage.

Array and struct initialisers, at both storage durations: `int a[3] = {1,2,3}`,
`struct P p = {1,2}`, nested and mixed, with a short list zeroing what it does
not reach and an unsized array taking its length from what it was given.

Pointers to functions: `int (*f)(int, int)` declared, assigned, passed, held in
an array and called, with a function's name converting to a pointer on its own
so that `qsort(a, n, s, cmp)` reads as it should. `qsort` and `bsearch` are in
the shipped `<stdlib.h>` because of it.

Declarators are recursive, so `int (*p)[10]` is a pointer to an array where
`int *p[10]` is an array of pointers — and abstract ones work too, which is what
makes `sizeof(char[8])` and the cast `(int (*)[4])malloc(...)` possible. That
cast is the whole of what a dynamically allocated matrix needs; `malloc` itself
arrives through an ordinary prototype.

`struct`, `union`, `enum` and `typedef`, with C's layout and padding rules,
`s.m` and `p->m`, whole-object assignment, and self-reference — a linked list
compiles, built in a static pool. It was written before anything shipped a
`malloc` prototype and has not been rewritten to use one, because a test that
still passes unchanged after the language grew under it is worth more than a
tidy one.

The preprocessor: `#define` and `#undef` for macros both object-like and
function-like — with `#`, `##`, calls that may span lines, and `__VA_ARGS__` —
both spellings of `#include`, where `"file"` starts beside the including file
and falls back to the search path while `<file>` uses the path alone,
the whole conditional family — `#ifdef`, `#ifndef`, `#if`, `#elif`,
`#else`, `#endif` — with real expressions in `#if`, plus `__FILE__`, `__LINE__`
and `#error`. It emits text rather than tokens so that a file using no directive
reaches the lexer byte for byte unchanged, and carries a line map so a message
about an included file still names that file.

`const` and `volatile`, and `static` on a local — which lives in the data
section, keeps its value between calls, and is initialised once by a constant.
`const` is checked on the object: `=`, `+=` and `++` all refuse one through the
same check. `volatile` is accepted and changes nothing, which is honest here —
every value already goes to memory and back on each access, so there is no
caching for it to forbid.

Bit-fields, named and unnamed, packed into a storage unit of their declared type
from the low bit up and never straddling one of its boundaries. Reading is two
shifts, which is what makes a signed 3-bit field holding 7 read back as −1;
writing is a read-modify-write, because the neighbours have to survive. `&f.a`
and `sizeof f.a` are refused, as C requires — a bit-field is an lvalue with no
address, and `genAddr` refuses one rather than handing back its storage unit.

Statements: `if`/`else`, `while`, `do`/`while`, `for`, `break`, `continue`,
`return`, blocks, and the empty statement. Expressions: arithmetic, comparison,
shifts, `%`, the short-circuiting `&& || !`, the bitwise `& | ^ ~` at C's own
precedence, compound assignment in all ten forms, and `++` / `--` in both positions.

`switch`, with `case` and `default`, falling through from one case to the next
and taking `break` to stop. It lowers to a chain of comparisons rather than a
jump table, which is a decision about when to optimise and not about what the
language means.

`goto` and labels, with the function scope C gives them — so a `goto` may name a
label further down, which is the ordinary use, since leaving two nested loops at
once is the thing `break` cannot do.

`c ? a : b`, evaluating only the arm it takes and bringing both arms to one
type, so `n ? 1 : 2.5` is a `double` even when the `int` arm is the one taken.

The comma operator, so `for (i = 0, j = n; i < j; ++i, --j)` works — and
declarations of several names at once, `int x, *p = &x, a[4];`, at file scope
and inside a function. The commas separating call arguments and declarators are
not the operator, which is the distinction C draws by calling an argument an
assignment-expression.

Integer constant expressions, through one evaluator shared by the four places
that need one: `case 1 + 2`, `enum { N = 1 << 4 }`, `char buf[sizeof(int) * 4]`
and `int g = 6 * 7`. The constant is parsed as an ordinary expression and then
folded, so the type checker has already run over it and `case 'a' + 1` needs no
rule of its own.

## The keywords

ANSI C has 32 keywords and this compiler now accepts **all 32**. The table is
here because a prose list of what is missing rots silently — twice already in
this file — while a table of every keyword can be checked by compiling one
program per row, which is how the column below was filled in.

| | | | |
| --- | --- | --- | --- |
| `auto` | `break` | `case` | `char` |
| `const` | `continue` | `default` | `do` |
| `double` | `else` | `enum` | `extern` |
| `float` | `for` | `goto` | `if` |
| `int` | `long` | `register` | `return` |
| `short` | `signed` | `sizeof` | `static` |
| `struct` | `switch` | `typedef` | `union` |
| `unsigned` | `void` | `volatile` | `while` |

Accepted is not the same as honoured, and three rows are worth the distinction.
`volatile` parses and changes nothing, because a stack machine writes every
value to memory already and there is no caching for it to forbid. `register`
parses and changes nothing either, but it carries the one rule it has: a
register object has no address, so `&x` on one is refused. `auto` is the default
storage class for a local and means nothing anywhere else, so it is refused at
file scope and on a parameter, where C does not allow it.

What used to be left is not a keyword either, and both are written now.
`long double` is a type spelled with two of them, and defining a variadic
function is a use of `...` rather than of a word. This file said they were
refused for longer than they were, which is the rot the table above exists to
prevent and did not.

Two things here are **not** ANSI C and are worth naming rather than leaving to
be discovered: variadic macros, and a declaration in a `for` header. Both are
C99, both are in the test suite, and both are extensions this compiler chose
rather than accidents.

## Missing and conspicuous

**The system's own headers.** `#include <stdio.h>` works and finds the header
this compiler ships in `lib/`, not `/usr/include/stdio.h` — which on the box is
24 files and 3,997 lines carrying 164 uses of `__attribute__` and `__restrict`
before it reaches a declaration this compiler could use. (`gcc -E -H` for the
file count, `wc -l` over the distinct headers it names for the rest; this file
said 744 lines for a long time and that number does not reproduce by any method
tried.) That is a shield and a limit at once: a program reaching for anything
outside the fifteen C90 headers will not build.

**Qualifiers as part of the type.** `const` here qualifies the object, so
`const char *s` leaves `*s` writable. This is the largest thing left.

**K&R function definitions, and trigraphs.** Both are C90 and both are refused,
so this is not a strictly conforming C90 implementation. They are declined
rather than pending: C23 deleted both, and trigraphs would silently change what
existing correct programs mean — `printf("What??!")` would start printing
`What|`. `tests/c90-probe.sh` reads 29 of 31, and these are the two.

**Unwind data on the GNU Windows path.** `.pdata` and `.xdata` are emitted for
`x86_64-windows` through MASM, which is the default. `-masm=gnu` writes none,
because GAS built for ELF rejects `.seh_*` outright and that output is
assembled on Linux by the cross-check suite.

Everything else this section used to list has been written, and several of them
were never as far away as it said. Initialisers for arrays and structs sat here
after they worked. So did the abstract array declarator, while `sizeof(char[8])`
was already answering 8. `auto` was not listed at all, while being the one
keyword of the thirty-two that did not work. And most recently this section
claimed only the Linux backend existed, and that `long double` and the variadic
definition were refused, while all three targets were passing their suites and
both features were in the corpus.

That is four times this list has been found wrong by measurement rather than
right by reading, which is the argument for `tests/c90-probe.sh` and
`tests/not-c90-probe.sh`: a prose list of what is missing rots silently, and a
probe that compiles one program per claim does not.

## Where it stands

[`docs/STATUS.md`](docs/STATUS.md) is the detailed account: what the language
accepts today, how the type system and code generator are built, what is
refused and by what message, how the 412 cases are distributed, and which of
the four staged parts are done. All four are.

[`help/command-lines.md`](help/command-lines.md) is the other half: what to
type to get a `.s`, an object or a program, for each of the three targets, and
which of those each can reach from the machine you are sitting at.

[`demo/README.md`](demo/README.md) walks one program from source to assembly to
answer, with the emitted `.s` kept in the repository so it can be read without
running anything first.

## Design

[`docs/TYPES.md`](docs/TYPES.md) settles the type system before any of it was
built: the model, what the `Target` owns rather than the front end, the
conversion rules, what code generation has to do differently once a value is
not always eight bytes, and the four stages it landed in.

[`docs/PARALLELISM.md`](docs/PARALLELISM.md) is about compiling with threads:
what can be done concurrently, what cannot — parsing one file cannot, because
`(A)*b` needs the symbol table built by everything before it — and the one
small change worth making early so that parallelising the back end is later a
scheduling change rather than a redesign.
