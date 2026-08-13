# What is implemented

Where the compiler stands. Taken from the code rather than from memory, and
meant to be re-derived the same way when it drifts — every number here is
countable from the repository.

Companion documents: [`TYPES.md`](TYPES.md) settles the type system and its
staging; [`../demo/README.md`](../demo/README.md) walks two programs from
source to assembly to answer.

---

## Scale

**4,153 lines of C++ in 14 files**, built by `g++` under
`-Wall -Wextra -Werror -pedantic`. **267 test cases**, all passing.

| File | Lines | Does |
| --- | --- | --- |
| `Parser.cpp` / `.h` | 1,880 | parsing, type checking **and** constant folding — C cannot separate the first two |
| `CodeGen.cpp` / `.h` | 913 | x86-64 System V, GNU as syntax |
| `Ast.h` | 504 | the node hierarchy and the visitor |
| `Type.cpp` / `.h` | 317 | types, interning, and the `Target` |
| `Lexer.cpp` / `.h` | 262 | text to tokens |
| `Driver.cpp` / `.h` | 191 | arguments, and one independent job per input file |
| `Source.cpp` / `.h` | 75 | the text, and every diagnostic |
| `main.cpp` | 11 | nothing but a way in |

The compiler emits assembly only. `gcc` assembles and links it, which keeps the
surface under test to the part being written.

---

## The language accepted

### Types

`struct` and `union`, with C's layout rules: each member at the next offset
that is a multiple of its own alignment, the whole rounded up to the widest
member's alignment so an array of them stays aligned. A union puts every member
at zero. `enum`, whose enumerators are `int` constants. `typedef`.

`void`. `char`, `signed char` and `unsigned char` — three distinct types, even
though one shares a representation with another. `short`, `int`, `long`,
`long long`, each with an unsigned form. `float` and `double`.

Pointers to anything, arrays of anything, and both at once: `int *p[10]` is an
array of ten pointers. Multi-dimensional arrays. Arrays decay to pointers
wherever they are used as values, and do not under `sizeof` or `&` — so
`char s[16]` measures 16 at its definition and 8 as a parameter, because a
parameter declared as an array is a pointer.

### Declarations

Locals, parameters, and file-scope objects. `static` gives internal linkage;
`extern` declares an object defined in another unit and emits nothing. Globals
may take an integer constant initialiser.

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
'const' is not supported yet
'register' is not supported yet
a storage class on a local is not supported yet
an array initialiser is not supported yet
defining a variadic function is not supported yet
more than 6 parameters is not supported yet
a struct or union in '?:' is not supported yet - use a pointer to it
a bit-field is not supported yet - 'a' cannot be given a width
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
```

Absent from the grammar: parenthesised declarators, so `int (*p)[10]` — a
pointer to an array — cannot be written, though `int *p[10]` can. Abstract
declarators with an array, so `sizeof(char[8])` cannot be written either,
though `sizeof(char *)` can.

Refused with a message: postfix `++` and `--`, which need a temporary the
compiler cannot yet make - the prefix forms work.

Not started: the preprocessor, and bit-fields.

A bit-field is only ever a member declarator; C has no such thing as a
free-standing one, so the grammar change lands in the struct and union member
rule and nowhere else. The rest of it is not a grammar change at all. Layout has
to pack fields into a storage unit and decide when one is straddled, `Member`
needs a width and a bit offset beside its byte offset, and every read and write
becomes a mask and a shift rather than a move of whole bytes — so it touches
`Type`, the layout code and `CodeGen` together, and only the front door is
small.

The part that is not merely unwritten is worth naming. **A bit-field is an
lvalue with no address** — C forbids `&f.a`, and gcc says so in as many words —
while everything here that reads or writes a place goes through `genAddr`, which
assumes there is one. Bit-fields do not extend that invariant, they contradict
it, which is the argument for adding them before more is built on top of it
rather than after.

Only the `X86_64Linux` target exists — Windows and Apple arm64 are designed for
but not written.

---

## How it is verified

Every case is compiled twice, by `cc1` and by `gcc`, both binaries are run
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
| Arithmetic, variables, and the early whole programs | 24 |
| **Total** | **267** |

Each increment ends with a deliberate injection — the compiler is broken on
purpose and the suite must notice — because a suite that has never failed is
unproven. Two lessons from doing that are recorded rather than forgotten:

- **An exit status carries one byte.** Breaking pointer scaling left a test
  passing because its wrong answer was congruent to the right one modulo 256.
  Cases must compare exactly or print; returning an aggregate is the weakest
  assertion available here.
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

What is left is not the type system. It is the preprocessor, bit-fields, and the
two targets that were designed for but never written.
