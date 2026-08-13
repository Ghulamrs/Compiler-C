# ansicc

An ANSI C compiler, written by hand in C++, targeting x86-64 System V.

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
| `lib/*.h` | the library it ships, which is not the language: `stddef.h`, `stdio.h`, `stdlib.h`, `string.h` |
| `src/Lexer.cpp` | source text → tokens |
| `src/Parser.cpp` | tokens → tree, recursive descent — **and** type checking, which C cannot separate from parsing, and the constant folder that four parts of the grammar need |
| `src/CodeGen.cpp` | tree → x86-64 assembly, GNU as syntax |
| `src/Type.cpp` | the type model, interning, and the `Target` that owns every size |
| `src/Ast.h` | the node hierarchy and the visitor |
| `src/Source.cpp` | the text, and every diagnostic |
| `src/Driver.cpp` | one job per input file, on threads at four or more — asking the machine how many cores it has; `main.cpp` is nothing but a way in |

6,098 lines of C++ in 16 files, under `-Wall -Wextra -Werror -pedantic
-pthread`, plus 190 lines of C in the four headers it ships.

Assembling and linking are left to `gcc`. That keeps the surface under test to
the part actually being written, and it is what makes the differential suite
below possible.

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

`tests/challenge.sh` is not a test but a stopwatch: it compiles the corpus a
hundred times under `cc1 -j 1`, `cc1 -j 4` and `gcc -O0 -S`, one invocation each
so the comparison is like for like, and requires every one of those runs to
produce the same assembly. Over 432 000 generated lines `cc1` comes out 11.5x
faster than `gcc -O0`, and the threads change it by -2.4% — which is this
machine's SMT ceiling and not the loop's fault.

Comparing against gcc rather than against expectations alone is the point: an
expectation is an opinion about C, while gcc is the reference implementation
sitting on the same disk. Where they disagree, the case is wrong until the
standard says otherwise. That has already caught four wrong expectations of
mine rather than compiler bugs.

**374 cases, all passing** — 366 single files, 7 directories, and one check on the
driver's threaded job loop. They run in parallel, because they are independent
and because the work is not this compiler — `cc1` accounts for about 0.3s of
the 12s a full run takes, and the rest is gcc assembling, gcc building the
reference, and running two binaries per case. Output is collected per case and
printed in name order, so a parallel run reads exactly like a serial one.

## Accepted today

Functions with prototypes and typed parameters, up to six of them, with
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
precedence, compound assignment in all ten forms, and prefix `++` / `--`.

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

## Missing and conspicuous

The system's own headers. `#include <stdio.h>` works and finds the header this
compiler ships in `lib/`, not `/usr/include/stdio.h` — which is 24 files and
744 lines of `__restrict` and `__attribute__` before it reaches a declaration
this compiler could use. Qualifiers as part of
the type — `const` here qualifies the
object, so `const char *s` leaves `*s` writable. Postfix `++` and `--`,
which need a temporary the compiler cannot yet make. A pointer to a function,
`int (*f)(void)`. Passing or returning a struct by
value. `long double`. Initialisers for arrays and structs. Defining a variadic
function. Only the `X86_64Linux` target exists; Windows and Apple arm64 are
designed for but not written.

Each of these is refused with a line number rather than mis-parsed, and all but
one are refused by name. The exception is the abstract array declarator, which
is simply absent from the grammar and so reports a missing token instead of
naming the rule. See [`docs/TYPES.md`](docs/TYPES.md) for the staging.

## Where it stands

[`docs/STATUS.md`](docs/STATUS.md) is the detailed account: what the language
accepts today, how the type system and code generator are built, what is
refused and by what message, how the 374 cases are distributed, and which of
the four staged parts are done. All four are.

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
