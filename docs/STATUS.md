# What is implemented

Where the compiler stands. Taken from the code rather than from memory, and
meant to be re-derived the same way when it drifts — every number here is
countable from the repository.

Companion documents: [`TYPES.md`](TYPES.md) settles the type system and its
staging; [`../demo/README.md`](../demo/README.md) walks two programs from
source to assembly to answer.

---

## Scale

**5,914 lines of C++ in 16 files**, built by `g++` under
`-Wall -Wextra -Werror -pedantic`, plus **116 lines of C in 4 shipped headers**.
**361 single-file cases and 7 multi-file ones**, all passing.

| File | Lines | Does |
| --- | --- | --- |
| `Parser.cpp` / `.h` | 2,322 | parsing, type checking **and** constant folding — C cannot separate the first two |
| `CodeGen.cpp` / `.h` | 1,010 | x86-64 System V, GNU as syntax |
| `Ast.h` | 545 | the node hierarchy and the visitor |
| `Type.cpp` / `.h` | 332 | types, interning, and the `Target` |
| `Lexer.cpp` / `.h` | 262 | text to tokens |
| `Driver.cpp` / `.h` | 231 | arguments, the include search path, and one independent job per input file |
| `Preprocessor.cpp` / `.h` | 1,093 | includes, conditionals and macros, before the lexer |
| `Source.cpp` / `.h` | 108 | the text, the line map, and every diagnostic |
| `main.cpp` | 11 | nothing but a way in |

The compiler emits assembly only. `gcc` assembles and links it, which keeps the
surface under test to the part being written. So it is `cc1 hello.c -o hello.s`
and then `gcc hello.s -o hello`: what `-o` names is never a program, and
`chmod +x` on it hands C to the shell, which reports every line of it as a
command it cannot find.

Both messages the driver can produce — the usage line and the unknown-option
refusal — name `argv[0]` rather than the literal string `cc1`, echoed verbatim
rather than trimmed to a basename. A compiler built by hand under another name
is the ordinary case, and one built as `cpp` shares its name with the system
preprocessor on the `PATH` — which accepts the same `-o`, writes something
plausible and exits 0. A diagnostic reading `cc1:` when the user typed `./cpp`
removes the last signal that these are two different programs. Trimming to the
basename would make two binaries print the same word again, which is the thing
being avoided.

---

## The language accepted

### Types

**Bit-fields**, named and unnamed, in structs and unions. `unsigned int a : 3;`
packs into a storage unit of its declared type, from the least significant bit
up. A field never straddles a boundary of that type — if it would, it starts at
the next one and the gap becomes padding. `int : 0;` names nothing and forces
the next field to a fresh unit, which is the only way to pad deliberately.

Reading one is two shifts and no mask: left until the field's top bit is the
register's top bit, then right, arithmetically when the member is signed. That
is what makes a signed 3-bit field holding 7 read back as −1. Writing one is a
read, a modify and a write, because the neighbours in the unit have to survive —
and `(f.a = 300)` on a 3-bit field is 4, both as what is stored and as the value
of the assignment.

The rule bit-fields break is the interesting part. **A bit-field is an lvalue
with no address**: `&f.a` and `sizeof f.a` are both refused, as C requires, and
`genAddr` — the one path everything else here uses to find a place — refuses one
outright rather than quietly handing back the address of its storage unit. That
would not be a smaller lie; every caller would then read and write the
neighbours too.

`struct` and `union`, with C's layout rules: each member at the next offset
that is a multiple of its own alignment, the whole rounded up to the widest
member's alignment so an array of them stays aligned. A union puts every member
at zero. `enum`, whose enumerators are `int` constants. `typedef`.

`void`. `char`, `signed char` and `unsigned char` — three distinct types, even
though one shares a representation with another. `short`, `int`, `long`,
`long long`, each with an unsigned form. `float` and `double`.

Pointers to anything, arrays of anything, and both at once. `int *p[10]` is an
array of ten pointers and `int (*p)[10]` is a pointer to an array of ten,
because **declarators are recursive**: the suffix binds tighter than the prefix,
and the parentheses are what undo that.

The parenthesised part is read twice. Once against a placeholder type, purely to
find its matching `)`, with the result thrown away; then the suffix after the
`)` is applied to the base, giving the type the inner declarator really
modifies, and the parser rewinds and reads the same tokens again for real. What
is inside the parentheses cannot be known to be a declarator *of* anything until
what follows them has been read, and no left-to-right scan gets there.

The same rule gives abstract declarators — a type with no name in it — so
`sizeof(char[8])` and the cast `(int (*)[4])` both work. That cast is what turns
a `malloc`ed block into a matrix, and `malloc` itself needs nothing new: it
arrives through an ordinary prototype. Multi-dimensional arrays. Arrays decay to pointers
wherever they are used as values, and do not under `sizeof` or `&` — so
`char s[16]` measures 16 at its definition and 8 as a parameter, because a
parameter declared as an array is a pointer.

### Declarations

One declaration may declare several names, each with its own declarator and its
own initialiser: `int x, *p = &x, a[4];` shares only the specifiers, so the `*`
and the `[4]` belong to one name apiece. A later initialiser sees the names
declared before it, as C requires for `int a = 1, b = a + 1;`.

The comma that separates them is not the comma operator, and neither is the one
between call arguments. C draws that line by calling an argument an
*assignment-expression*, and this parser draws it the same way — `assign()`
where a comma separates, `expr()` where it operates. Get it wrong and `f(1, 2)`
becomes a call with one argument, which is exactly what the injection below
provokes.

`static` on a local gives it static storage duration: the object lives in the
data section rather than the frame, keeps its value between calls, and is
initialised once by a constant before the program runs — so no statement is
produced for it. Its scope is still the block. Two blocks of one function may
each declare `static int n`, and they are two objects; the data-section symbol
is the function's name, the variable's, and a number when that is not enough.

`const` and `volatile` are accepted. `const` is a property of the declared
object rather than of its type, and every write goes through one check, so `=`,
the compound assignments and `++` all refuse a const object by the same rule.

**What that does not catch is worth stating.** `const` is not part of the type
here, so `const char *s` makes neither `s` nor `*s` read-only — the qualifier
belongs to the pointee, and this model has no pointee qualifiers. Making
`const char *` a distinct interned type from `char *` would reach every
comparison in the parser, since assignment, calls and `?:` all decide
compatibility by pointer equality on interned types. The direct case is checked;
the case through a pointer is accepted, which is a missing check and not a wrong
answer.

`volatile` is accepted and changes nothing, and that is honest rather than lazy:
this is a stack machine with no register allocator, so every value is written to
memory and read back on each access already. There is no caching for `volatile`
to forbid. It would start to mean something the day values live in registers
across statements.

Locals, parameters, and file-scope objects. `static` gives internal linkage;
`extern` declares an object defined in another unit and emits nothing. Globals
may take an integer constant initialiser.

A prototype may name only types — `int printf(char *, ...);` — which is how a
header is written, and now that `#include` exists it is how the files this
compiler reads will be written too. A definition may not: a body cannot use what
it cannot name, and that is refused by name. An array parameter may leave its
length out, `char s[]`, since it is a pointer either way; only the outermost
dimension may go, because the others are what decide how far one step moves.

**A prototype is mandatory.** An undeclared name is refused rather than assumed
to return `int`, and every call is checked against its signature: the number of
arguments, the type of each, and the return type. A definition that contradicts
its own prototype is refused, as is a function defined twice.

### Expressions

| | |
| --- | --- |
| Arithmetic | `+ - * /`, `%` on integers only |
| Shifts | `<<`, `>>` — arithmetic or logical by signedness |
| Comparison | `== != < <= > >=`, yielding `int` valued 0 or 1 |
| Logical | `&& \|\| !`, short-circuiting: `0 && f()` does not call `f` |
| Assignment | `=`, right-associative, to any lvalue |
| Pointers | `&x`, `*p`, `a[i]`, and arithmetic that scales by the element |
| Members | `s.m` and `p->m`, the second lowered to `(*p).m` |
| Bitwise | `& \| ^ ~`, at C's precedence - `a & b == c` is `a & (b == c)` |
| Compound | `+= -= *= /= %= &= \|= ^= <<= >>=`, and prefix `++` / `--` |
| Comma | `a, b` — evaluates `a` for its effects, discards it, and takes `b` |
| Conditional | `c ? a : b`, evaluating one arm, both brought to one type by the usual arithmetic conversions — so `n ? 1 : 2.5` is a `double` even when the `int` arm is taken |
| Other | function calls, `sizeof` on a type or an expression, casts |
| Literals | decimal, hex and octal integers with `u`/`l` suffixes; `1.5`, `1.5f`; `'a'` (an `int`); `"text"` (a `char[N+1]`) |

### Statements

`return`, `if`/`else`, `while`, `do`/`while`, `for`, `break`, `continue`,
blocks, expression statements, and the empty statement.

`switch`, with `case` and `default`. The controlling expression is promoted and
every case value is converted to that promoted type, so a `switch` on a `char`
compares in `int` and two labels that reach the same value after conversion are
caught as duplicates — `case -1` and `case 4294967295u` are the same label on an
`unsigned int` switch, and saying so is the whole reason the conversion happens
in the parser.

A case may sit anywhere inside the body, including in a nested block, and
control falls from one into the next unless something stops it. `break` leaves
the switch; `continue` inside a switch looks past it to the enclosing loop,
which is the only thing that distinguishes the two statements.

It lowers to a chain of comparisons, not a jump table. A table wants the values
sorted and the span weighed against the count, and that is worth writing when
there is a program slow enough to measure it against.

`goto`, and labels. Labels have function scope rather than block scope, so a
`goto` may name one that has not been parsed yet — and the forward jump is the
ordinary use, since leaving two nested loops at once is exactly what `break`
cannot do. The names are therefore collected as the body is parsed and checked
against each other when the function closes, which is the first moment the
answer exists. Two functions may each have a `done:`; the assembler label
carries the function's name, so they are two labels and not one defined twice.

### The preprocessor

A stage before the lexer, producing the translation unit the rest of the
compiler sees: `#include` spliced in, conditionals resolved, macros expanded.

`#define` and `#undef`, object-like and function-like, expanded recursively so
one may be written in terms of another — and not re-expanded inside their own
expansion, which is what makes `#define N (N_BASE + 1)` terminate.

A function-like macro is invoked only when a `(` follows, so `MAX` alone is an
ordinary identifier and a variable may share the name. Whether the macro *is*
function-like is decided by a space: `#define A (x)` has a body beginning with a
parenthesis, `#define A(x)` takes a parameter, and nothing but that space
distinguishes them.

**Arguments are expanded in the caller's context, before the macro is marked as
being expanded.** That ordering is C's rule and not a detail — it is what makes
`MAX(MAX(1, 9), 2)` expand the inner call. Getting it the other way round leaves
the inner `MAX` standing, which is exactly what the first version of this did.
Only the replacement list is rescanned with the name blocked, and that is what
ends the recursion.

`#` takes an argument's spelling rather than its expansion, `##` joins what is
on either side of it before anything else looks at either, and a call may be
written across as many lines as it likes — the lines are pulled in until the
parentheses balance.

**Variadic macros**, `#define LOG(fmt, ...)`, with `__VA_ARGS__` carrying the
arguments past the named ones *and the commas that separated them*, since that
is what C defines it to be. `#__VA_ARGS__` gives the whole variable part as it
was written.

Two admissions about this one. It is **C99, not ANSI C**, and this compiler
claims to be the latter — it is here because the alternative to a variadic macro
in real code is no macro at all. And `, ## __VA_ARGS__`, which deletes the comma
before it when there is nothing to put after it, is **GNU's rule rather than
C's**. Without it the idiom that motivates the whole feature — a macro taking a
format and then possibly nothing — expands to `printf("...",)` and does not
compile. Both are extensions, and calling them anything else would be a claim
this compiler cannot support. `#include "file"`,
resolved beside the including file and nested to a depth of 32. `__FILE__`,
`__LINE__`, `#error`, and `#pragma` ignored as C permits.

The whole conditional family: `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`,
`#endif`, nested. `#if` and `#elif` take a real expression, which needs **a
second constant expression evaluator** — the parser's cannot be used, since this
runs before a token stream, a symbol table or a type exists. `defined X` is
resolved first, then macros are expanded, then any name still standing is 0,
which is C's rule and why `#if NEVER_DEFINED` is false rather than an error.

**It emits text, not tokens, and that decided the design.** Every diagnostic
here is a byte offset into a `Source`, and three hundred tests depend on the
exact output of `Source::fail`. Emitting text means a file with no directives
comes through byte for byte unchanged, so nothing about the existing diagnostics
moved. The cost is that offsets no longer point where the code was written once
an include is spliced in, and that is paid for with a line map: one entry per
emitted line saying which file and line it came from, which `Source` consults so
a message about an included file names that file.

Substitution is not done on raw text. The scanner knows string literals,
character constants and both kinds of comment, so a macro named `n` does not
rewrite the middle of `"an error"` or of a comment. That is the one part of a
text-level preprocessor that must be token-aware to be correct at all.

Both spellings of `#include`, and the difference between them is the whole
reason there are two. `"file.h"` starts beside the file that wrote the
directive and falls back to the search path; `<file.h>` uses the search path
alone and never looks beside the including file — so a header named `string.h`
sitting next to a source file cannot quietly become the one `<string.h>` meant.
The search path is every `-I` in the order given, then the directory of headers
this compiler ships, so a `-I` shadows a shipped header and never the reverse.

A header that is not found reports every path it tried, in order, because a
missing header is nearly always a header looked for somewhere other than where
it sits:

```
cannot find <nosuch.h> - looked in /home/ec2-user/ansicc/include/nosuch.h
```

Not supported: GNU's named variadic parameter `args...`, refused by name.

Two mistakes are caught where they are written rather than where they are used:
a `#` not followed by one of the macro's own parameters, and a parameter list
with a comma and nothing after it. A definition nobody calls would otherwise
never be checked at all.

### The headers it ships

`include/` holds `stddef.h`, `stdio.h`, `stdlib.h` and `string.h` — 116 lines of
ordinary C, found through the search path baked in at build time from
`$(CURDIR)`, so a clone built elsewhere finds its own and not this one's.

**They are not glibc's headers and not copies of them.** Reaching the real
`<stdio.h>` means reading 24 files and 744 lines carrying 107 uses of
`__restrict`, 57 of `__attribute__`, 15 of `long double` and a scattering of
`__extension__`, `__asm__`, `__inline` and `__typeof` — none of it C, and all of
it in the way before a single usable declaration. What a program wants from
`stdio.h` is the prototypes, and a prototype is ordinary C.

The declarations must agree with glibc's, because the program links against
glibc, and **nothing here is taken on trust**. The suite deliberately does not
pass `-I` to gcc: every case is built a second time against the real headers,
and the two binaries must produce the same bytes and the same exit status. A
prototype that lied would fail there rather than pass quietly. That is also what
checks `size_t` — typedefed as `unsigned long` in text, since a header cannot
consult `Target`, and confirmed by a case printing `sizeof(size_t)` under both.

What is absent is `FILE` and everything taking one. An opaque handle needs an
incomplete type, which this compiler does not have.

### Constant expressions

`case 1 + 2`, `enum { N = 1 << 4 }`, `int a[2 * 5]`, `char buf[sizeof(int) * 4]`
and `int g = 6 * 7` all work, and all go through one evaluator.

It is not a second grammar. The constant is parsed as an ordinary conditional
expression and then folded, so the type checker has already run over it before
the folder sees anything — the promotions and conversions are `Cast` nodes in
the tree, which is why `case 'a' + 1` needs no rule of its own. The `Cast` is
where the width and the signedness of the answer are decided, exactly as it is
everywhere else here.

Anything the folder does not recognise is not a constant. That is the safe
direction to be wrong in: a missed fold is a refusal with a message, never a
wrong number. Only the arm of a `?:` that is taken has to fold, which makes
`sizeof(long) == 8 ? 8 : 4` the idiom it is meant to be.

### Functions

Definitions, up to six parameters, recursion and mutual recursion. Calls into
libc given a prototype, so `putchar`, `puts` and `printf` all work. Variadic
*prototypes* — `int printf(char *fmt, ...);` — though a variadic function
cannot yet be defined.

---

## How the type system is built

**Sizes are never literals in the front end.** `Target` answers every size,
alignment and signedness question, because `sizeof(long)` is 8 here and 4 on
Windows, and a hardcoded 8 would make the compiler quietly wrong on one target
while the tests on the others stayed green.

**Types are interned** — one object per distinct type, so equality is a pointer
comparison even for a type reached by two different routes.

**Every conversion is an explicit `Cast` node** the parser inserts: the integer
promotions, the usual arithmetic conversions, assignment, prototyped arguments,
and the default argument promotions past a variadic's named parameters. Code
generation knows no conversion rule; it only widens and narrows.

**Signedness selects the instruction**, not merely the type — `movsbq` against
`movzbq`, `idiv` against `div`, `sar` against `shr`, `setl` against `setb`. So
`-1 < 1u` is false, `-1L < 1u` is true on LP64, and `-1 >> 1` stays `-1` where
the unsigned shift gives 2147483647.

---

## How code is generated

A stack machine, deliberately: every intermediate goes through the stack, which
is always correct and produces poor code. Register allocation is a later and
separable problem.

An expression leaves its value in `%rax` if its type is integer or a pointer,
and in `%xmm0` if it is floating — never guessed, the type says. An integer in
`%rax` is always held sign- or zero-extended to 64 bits for its own type.

Every lvalue can produce its address, which is what allows `*p = x` and
`a[i] = x`; `&` uses the same path and reads nothing.

Frames are laid out by size and alignment rather than eight bytes per local.
System V classifies each argument INTEGER or SSE and counts the two lanes
independently, so `f(1, 1.5, 2, 2.5)` puts 1 and 2 in `%rdi` and `%rsi` while
1.5 and 2.5 go in `%xmm0` and `%xmm1`. `%al` carries the number of vector
registers used, which a variadic callee reads.

---

## Not implemented

Refused by name, with a message and a line number:

```
'long double' is not supported yet
passing a struct or union by value is not supported yet - pass a pointer to it
returning a struct or union by value is not supported yet
'register' is not supported yet - it is a hint this compiler has no way to
  take, since every value already goes through memory
'extern' on a local is not supported yet - declare it at file scope
an array initialiser is not supported yet
defining a variadic function is not supported yet
more than 6 parameters is not supported yet
a struct or union in '?:' is not supported yet - use a pointer to it
```

Refused because the program is wrong rather than because the compiler is
unfinished:

```
no label 'x' in this function
label 'x' is defined twice in this function
a label cannot be followed by a declaration - put it in a block
the arms of '?:' have incompatible types 'int *' and 'int'
division by zero in a constant expression
shift count out of range in a constant expression
an array length must be positive, not -4
expected a case value, and this is not an integer constant expression
'a' is a bit-field, and a bit-field has no address
sizeof cannot be applied to 'a', which is a bit-field
'a' is 33 bits, which does not fit in 'unsigned int'
'a' has a bit-field width of -1, which cannot be negative
a bit-field must have an integer type, not 'double'
'k' is const and cannot be assigned to
a parameter of a definition needs a name - a prototype may leave it out, a
  body cannot
only the first dimension may be left empty - the others decide how far one
  step moves
```

Absent from the grammar: a pointer to a function, `int (*f)(void)`. The
declarator grammar reaches it and nothing in the type model could hold it, so it
is refused by name. A parenthesis that undoes nothing — `int (f)(void)` — is not
that, and is accepted.

Refused with a message: postfix `++` and `--`, which need a temporary the
compiler cannot yet make - the prefix forms work.

Not started: reading the system's own headers. `<stdio.h>` resolves to the one
this compiler ships, not to `/usr/include/stdio.h`, and pointing `-I` at
`/usr/include` would fail on the first `__attribute__` it met. That is a
different project from having a search path, and it is mostly a project about
absorbing GNU extensions rather than about C.

Only the `X86_64Linux` target exists — Windows and Apple arm64 are designed for
but not written.

---

## How it is verified

**Seven cases are directories rather than files**, under `tests/multi/`. Each is
compiled one unit at a time, linked, and compared against gcc's build of the
same sources — because separate compilation is the thing C is arranged around,
and no single-file case can reach it. That coverage did not exist until it was
asked for, and it found two bugs in its first run:

- `extern int x;` from a header followed by `int x = 0;` in the unit that owns
  the object was refused as a double declaration. It is the mechanism a header
  runs on. C allows any number of declarations and one definition, and that is
  now what happens.
- **`static` on a function did nothing.** Objects had internal linkage and
  functions did not, so two units with a `static` helper of the same name would
  not link. `demo/multifile` had claimed since it was written that "static is
  real rather than decorative"; for functions it was decorative.

`demo/multifile` is now one of those cases, reached by a symlink, so the program
its README describes is compiled and run by the suite rather than only read.

The seventh, `header_shadow`, is a directory for a different reason: it holds a
file called `string.h`, and no single-file case could. Three hundred cases share
one directory, and a `string.h` in it would be found by every one of them. Its
`main.c` includes both spellings of that name and needs them to mean different
files — `"string.h"` the one beside it, `<string.h>` the shipped one — which is
the only way the rule can be asserted rather than described. Making `<...>` look
beside the including file leaves `strlen` undeclared and the case stops
compiling.

Every single-file case is compiled twice, by `cc1` and by `gcc`, both binaries are run
under a five-second limit, and the case passes only when the two agree on **the
exit status and the printed output** and both match the `// expect:` line.

gcc is the reference implementation sitting on the same disk. An expectation is
an opinion about C; where the two disagree, the case is wrong until the
standard says otherwise. This has already caught a wrong expectation of mine
rather than a compiler bug — `fact(fact(3))` is 720, and an exit status kept
only 208 of it.

| Area | Cases |
| --- | --- |
| Types, conversions, signedness | 27 |
| Floating point | 25 |
| Logical operators and short circuit | 19 |
| Functions, calls, printed output | 17 |
| Expressions and control flow | 16 |
| Pointers | 11 |
| Arrays | 11 |
| Globals | 8 |
| Strings | 6 |
| Structs, unions, enums, typedefs | 26 |
| The toolkit program below | 1 |
| Loops, jumps, bitwise, compound assignment | 27 |
| `switch`, `case`, `default` | 18 |
| `goto` and labels | 6 |
| `?:` | 10 |
| Constant expressions | 15 |
| The comma operator and declarator lists | 11 |
| Bit-fields | 13 |
| The preprocessor | 17 |
| Function-like macros | 15 |
| Variadic macros | 8 |
| Unnamed parameters | 6 |
| Separate compilation (directories) | 7 |
| The shipped headers, and both spellings of `#include` | 6 |
| Parenthesised and abstract declarators | 7 |
| `const`, `volatile`, `static` locals | 11 |
| Arithmetic, variables, and the early whole programs | 24 |
| **Total** | **368** |

Each increment ends with a deliberate injection — the compiler is broken on
purpose and the suite must notice — because a suite that has never failed is
unproven. Two lessons from doing that are recorded rather than forgotten:

- **An exit status carries one byte.** Breaking pointer scaling left a test
  passing because its wrong answer was congruent to the right one modulo 256.
  Cases must compare exactly or print; returning an aggregate is the weakest
  assertion available here.
- **Argument evaluation order is unspecified, and this compiler does not match
  gcc's.** A case that printed two calls as arguments to one `printf` disagreed
  with gcc — not a bug in either, since C leaves the order unspecified, but a
  test that asserts one is testing nothing. One call per statement.
- **The stack alignment was unprovable until stage 3.** Deleting it changed
  nothing for eleven commits, because nothing the compiler could call cared.
  `printf` with a floating argument cares, and removing the padding now
  segfaults.

---

## The largest program it compiles

[`tests/cases/toolkit.c`](../tests/cases/toolkit.c) is 220 lines using nearly
everything above at once: a sieve counted through a `static` global, a bubble
sort done through a pointer with `swap(&a[j], &a[j+1])`, Newton's method in
`double` ending on a floating comparison, string reversal into a `char[32]`,
`factorial` in `long`, and a dozen `printf` calls mixing `%d`, `%ld`, `%u`,
`%s` and `%.8f`. It produces 25 lines of output **identical character for
character** to gcc's build of the same file.

The subset it was written in still shapes how it reads, and visibly: every loop
is a `while`, every counter advances with `i = i + 1`, and `is_prime` carries a
flag rather than returning from inside its loop. `for`, `++` and `break` all
exist now and it has not been rewritten to use them — a test that still passes
unchanged after the language grew under it is worth more than a tidy one.

Writing it found one thing. Its first `printf` took seven integer arguments,
one more than System V has registers for, and the refusal came from code
generation with no line number - "too many arguments for the registers", and
nothing about where. The limit is now checked in the parser, which has a
position to point at and can say which call and how many.

---

## Where this sits in the plan

[`TYPES.md`](TYPES.md) staged the type system in four parts. Three are done:

| Stage | | |
| --- | --- | --- |
| 1 | integer types, conversions, `sizeof`, casts | **done** |
| 2 | declarators, pointers, arrays, strings, globals | **done** |
| 3 | `float`, `double`, SSE, the variadic `%al` | **done** |
| 4 | `struct`, `union`, `enum`, `typedef` | **done** |

All four staged parts are done. The ambiguity flagged at the very beginning is
resolved the only way it can be: `atTypeName()` consults the typedef table, so
`(Byte)big` is a cast because `Byte` was typedefed, and the same text would be
a multiplication if it had been a variable. The grammar never decides it.

What is left is not the type system. It is qualifiers as part of the type rather
than of the object, and the two targets that were designed for but never
written.
