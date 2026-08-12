# The type system

What this compiler will consider a type, how types are written down, what
happens when two of them meet, and what code generation must do differently
once a value is no longer always eight bytes wide.

Nothing here is implemented yet. This document exists so the model is settled
before it is threaded through the lexer, the parser, every node of the tree and
every load and store — because retrofitting a type system is not a change, it
is a rewrite.

Where this document and the standard disagree, the standard wins. Where this
document and gcc disagree, gcc is probably right and the disagreement is worth
understanding before it is resolved.

---

## 1. The dialect

**Proposed: C89 as the baseline, plus four things.** This needs confirming.

| Kept from later standards | Why |
| --- | --- |
| `//` comments | Already accepted, already tested |
| Declarations anywhere in a block | Already accepted; C89 wants them at the top of a block |
| `long long` | Every target has it; a 64-bit type that C89 cannot name is a hole |
| Mid-file prototypes required before use | Already enforced, and deliberately stricter than C89 |

Everything else follows C89: no `_Bool`, no designated initialisers, no
variable-length arrays, no `restrict`, no compound literals.

The last row is worth restating because it is a *departure in the strict
direction*. C89 permits calling an undeclared function and assumes it returns
`int`. This compiler refuses, and the type system depends on that refusal: a
call whose prototype is unknown cannot have its arguments converted correctly,
because there is nothing to convert them to.

---

## 2. The model

Three axes, and a fourth thing that is not part of the type at all.

```
Type   = base × derivation × qualifiers
Symbol = Type + storage class + linkage + name
```

Conflating storage class with type is a common and costly mistake. `static int
x` and `int x` have **the same type**; they differ in lifetime and linkage. If
`static` were part of the type, two declarations of the same function would
compare unequal for no reason.

### 2.1 Representation

```cpp
enum class Kind {
    Void,
    Bool,                       // reserved; not in the C89 subset
    Char, SChar, UChar,         // three distinct types, see 3.2
    Short, UShort,
    Int, UInt,
    Long, ULong,
    LongLong, ULongLong,
    Float, Double, LongDouble,
    Pointer, Array, Function,
    Struct, Union, Enum
};

class Type {
public:
    Kind kind() const;
    bool isConst() const;
    bool isVolatile() const;

    // Derived types. Null unless the kind says otherwise.
    const Type *pointee() const;    // Pointer, Array
    long arrayLength() const;       // Array; -1 when incomplete, "char s[]"
    const Type *returns() const;    // Function
    const std::vector<const Type*> &params() const;
    bool isVariadic() const;

    // Answered by the Target, never by a constant here. See section 4.
    int size(const Target &) const;
    int align(const Target &) const;
    bool isSigned(const Target &) const;   // plain char depends on the target

    bool isInteger() const;
    bool isFloating() const;
    bool isArithmetic() const;              // integer or floating
    bool isScalar() const;                  // arithmetic or pointer
    bool isComplete() const;
};
```

**Types are interned.** One `Type*` per distinct type, owned by a `TypeTable`,
compared by pointer. Two reasons: `int*` appearing in forty places should be one
object, and structural comparison of recursive types (a struct containing a
pointer to itself) is a graph walk that pointer equality avoids entirely.

Types are immutable once made. The single exception is completing a struct:
`struct S;` then later `struct S { ... };` names the same type, which acquires
members. That is the one mutation the model allows and it is worth the
irregularity — forward references are how C describes a linked list.

---

## 3. Base types

### 3.1 The set

| Type | C89 | Notes |
| --- | --- | --- |
| `void` | yes | Incomplete. No size, no objects, no arithmetic |
| `char` | yes | A **third** type, distinct from both signed and unsigned char |
| `signed char`, `unsigned char` | yes | |
| `short`, `unsigned short` | yes | `short int` is the same type |
| `int`, `unsigned int` | yes | `signed`, alone, means `int`; `unsigned` alone means `unsigned int` |
| `long`, `unsigned long` | yes | `long int` is the same type |
| `long long`, `unsigned long long` | **no** | Added deliberately, see section 1 |
| `float`, `double`, `long double` | yes | |
| `struct`, `union`, `enum` | yes | Stage 4 |

Spellings collapse: `unsigned long int`, `long unsigned`, and `unsigned long`
are one type. The parser gathers specifiers into a set and maps the set to a
`Kind`; an illegal combination such as `long float` is rejected there, once,
rather than at each use.

### 3.2 Why `char` is three types

`char`, `signed char` and `unsigned char` are distinct types even though `char`
has the representation of one of the other two. `int f(char*)` and
`int f(signed char*)` are different functions. This bites in exactly one place
that matters — pointer compatibility — and costs nothing to model correctly
from the start.

Whether plain `char` is signed is **implementation-defined and target-specific**.
Measured on this box: signed. It must be asked of the `Target`.

---

## 4. Sizes belong to the Target

The single most consequential decision in this document.

Measured with gcc on the development box, and taken from the ABIs for the other
two:

| | Linux x86-64 | Windows x64 | Apple arm64 |
| --- | --- | --- | --- |
| `char` | 1 | 1 | 1 |
| `short` | 2 | 2 | 2 |
| `int` | 4 | 4 | 4 |
| **`long`** | **8** | **4** | **8** |
| `long long` | 8 | 8 | 8 |
| `float` | 4 | 4 | 4 |
| `double` | 8 | 8 | 8 |
| **`long double`** | **16** | **8** | **8** |
| pointer | 8 | 8 | 8 |
| `size_t` | `unsigned long` | `unsigned long long` | `unsigned long` |
| plain `char` | signed | signed | signed |
| model | LP64 | **LLP64** | LP64 |

`sizeof(long)` differing between Linux and Windows changes struct layouts, array
strides, and the result of every `long` computation. If a size is ever written
as a literal in the front end, the compiler is silently wrong on one of the
three targets and the tests on the other two will never say so.

So:

```cpp
class Target {
public:
    virtual int sizeOf(Kind) const = 0;
    virtual int alignOf(Kind) const = 0;
    virtual bool plainCharIsSigned() const = 0;
    virtual Kind sizeType() const = 0;      // what sizeof yields
    virtual Kind ptrDiffType() const = 0;
};
```

Alignment on all three targets equals size for scalars, with `long double` the
exception (16 on x86-64 Linux). Arrays take the alignment of their element, not
of their total size — `char[16]` is 16 bytes aligned to **1**, measured.

---

## 5. Derived types

### 5.1 Pointer

`T*` for any `T`, including incomplete ones. Always the target's pointer size.
Arithmetic on `T*` scales by `sizeof(T)`; `p + 1` moves by one element, not one
byte, and `void*` therefore has no arithmetic (gcc allows it as an extension,
treating `sizeof(void)` as 1 — we will not).

### 5.2 Array

`T[N]`. Size is `N * sizeof(T)`, alignment is `alignof(T)`. `N` may be absent
in a parameter or an `extern` declaration, giving an incomplete type.

**Decay** is the rule that surprises people. An array converts to a pointer to
its first element in every context except three:

1. as the operand of `sizeof` — `sizeof s` where `char s[16]` is **16**, not 8
2. as the operand of unary `&` — `&s` has type `char(*)[16]`
3. as the initialiser of an array — `char s[] = "abc"`

Consequently `char string[16]` as a *parameter* is not an array at all; it is
`char*`, and `sizeof` inside the function gives 8. This is worth a test on its
own, because it is the single most common misunderstanding in C.

### 5.3 Function

`T(params)`, with a flag for variadic. Function designators decay to function
pointers in every context except `sizeof` and `&`. Function pointer types are
part of the model from the start even though nothing will call through one until
later — leaving them out shapes the declarator parser wrongly.

---

## 6. Qualifiers

`const` and `volatile`, on any type, including through a pointer — and the
distinction between `const char *p` (pointer to const char) and
`char *const p` (const pointer to char) falls out of the declarator grammar
rather than being special-cased.

Qualifiers participate in type identity for assignment compatibility but not for
representation: `const int` and `int` are the same size and the same
instructions. What `const` buys is a diagnostic — assigning through it is an
error the parser can give with a line number.

`volatile` will be **parsed and recorded but not honoured** until there is an
optimiser to suppress, which there is not. Recording it now costs nothing;
adding it later would mean revisiting every declaration.

---

## 7. Storage class and linkage

Not part of the type.

| Keyword | Lifetime | Linkage | Where |
| --- | --- | --- | --- |
| *(none)*, file scope | static | external | global |
| *(none)*, block scope | automatic | none | stack slot |
| `static`, file scope | static | **internal** | `.data`/`.bss`, no `.globl` |
| `static`, block scope | **static** | none | `.data`/`.bss`, invisible by name |
| `extern` | static | external | declaration only, no storage |
| `auto`, `register` | automatic | none | accepted, ignored |

`static` at block scope is the interesting one: a local whose storage outlives
the call. It becomes a uniquely-named object in `.bss`, not a frame slot, and
`static int n = 0;` inside a function initialises **once**, not per call.

---

## 8. Declarators

C declarations read inside-out, and the grammar is genuinely awkward:

```c
int *a[10];      /* array of 10 pointers to int   */
int (*a)[10];    /* pointer to an array of 10 int */
int *f(void);    /* function returning int*       */
int (*f)(void);  /* pointer to function returning int */
```

The parse is a two-part job: gather **declaration specifiers** (base type,
qualifiers, storage class) left to right into a set, then parse the
**declarator**, which builds the derivation from the inside out. The recursive
shape:

```
declaration = specifiers declarator ("," declarator)* ";"
declarator  = "*"* qualifier* direct-declarator
direct-declarator = ident
                  | "(" declarator ")"
                  | direct-declarator "[" [constant] "]"
                  | direct-declarator "(" params ")"
```

The suffixes bind tighter than the prefix `*`, which is exactly why `*a[10]` is
an array of pointers and parentheses are needed to say otherwise.

### 8.1 The typedef ambiguity

`(A)*b` is a cast of `*b` to type `A` if `A` is a typedef name, and a
multiplication of `A` by `b` if it is a variable. **The grammar cannot decide
this; only the symbol table can.** The parser must therefore consult its scope
stack while parsing, which is the reason both symbol tables already live in
`Parser` rather than in a later pass. When `typedef` lands, the lexer's
`TK_IDENT` will need splitting into identifier and type-name at the point of
use, driven by that table.

---

## 9. Conversions — the part that is actually hard

Declaring types is bookkeeping. The conversions are where compilers are subtly
wrong, and where differential testing against gcc earns its place.

### 9.1 Integer promotions

Any `char`, `signed char`, `unsigned char`, `short`, `unsigned short`, or bit
field narrower than `int` converts to **`int`** if `int` can represent all its
values, otherwise to `unsigned int`. On all three targets `int` is 32 bits and
everything narrower fits, so the answer is always `int`.

Applied to: both operands of the arithmetic operators, the operand of unary
`+ - ~`, the shift operands **individually**, and variadic arguments.

### 9.2 The usual arithmetic conversions

Applied to binary `* / % + -`, the comparisons, `& ^ |`, and the two arms of
`?:`. In order:

1. If either is `long double`, the other becomes `long double`.
2. Otherwise if either is `double`, the other becomes `double`.
3. Otherwise if either is `float`, the other becomes `float`.
4. Otherwise, promote both, then:
   - same signedness → the higher rank wins;
   - unsigned rank ≥ signed rank → **unsigned** wins;
   - the signed type can represent every value of the unsigned type → signed wins;
   - otherwise → the unsigned version of the signed type.

Rank: `long long` > `long` > `int` > `short` > `char`.

This produces the classic:

```c
unsigned int u = 1;
int i = -1;
i < u        /* FALSE. i becomes 4294967295u */
```

and, only on LP64 where `long` is 64-bit:

```c
unsigned int u = 1;
long l = -1;
l < u        /* TRUE. long can hold every unsigned int, so u becomes signed */
```

The same two lines give **different answers on Windows**, where `long` is 32
bits and rule four's third clause no longer applies. A test for this belongs in
the corpus from the day the second target exists.

### 9.3 Assignment conversions

The right side converts to the type of the left. Arithmetic to arithmetic is
always allowed and may lose value silently (`char c = 300;` is
implementation-defined, not an error). Pointer to pointer requires compatible
pointee types, ignoring top-level qualifiers on the pointee only in the
permissive direction: `const char *p = q;` where `q` is `char*` is fine; the
reverse is a diagnostic.

`0` is a null pointer constant and converts to any pointer type. `void*`
converts to and from any object pointer without a cast — the one hole in
pointer type-checking that C deliberately leaves open.

### 9.4 Argument conversions

Two different rules, and using the wrong one produces garbage:

- **Within a prototype's fixed parameters**: the assignment conversions, to the
  declared parameter type.
- **Beyond the last named parameter of a variadic function**: the *default
  argument promotions* — integer promotions, and **`float` becomes `double`**.

This is why `printf("%f", 1.0f)` works: the `float` is promoted to `double`
before the call, and `%f` reads a `double`. It is also why passing a `char` to
`...` and reading it with `%c` works.

### 9.5 Casts

An explicit cast performs any conversion the language permits implicitly, plus
integer↔pointer (implementation-defined, permitted), and suppresses the
diagnostics that would otherwise fire. It never changes the bits of a pointer on
these targets.

---

## 10. The type of an expression

| Expression | Type |
| --- | --- |
| integer constant | smallest of `int`, `long`, `long long` that holds it, respecting any suffix and the base |
| `'a'` | **`int`**, not `char` — a genuine C surprise |
| `"abc"` | `char[4]`, which decays to `char*` almost everywhere |
| `1.5` | `double`; `1.5f` is `float` |
| `a + b` | the usual arithmetic conversions |
| `p + n`, `n + p` | type of `p`; the offset scales by `sizeof(*p)` |
| `p - q` | `ptrdiff_t`, and only for compatible pointers |
| `a < b`, `a == b` | **`int`**, valued 0 or 1 — never a boolean type |
| `a && b`, `!a` | `int`, 0 or 1 |
| `a = b` | the type of `a`, and the value is what was stored |
| `sizeof x` | `size_t`, and **`x` is not evaluated** |
| `&x` | pointer to the type of `x`; `x` must be an lvalue |
| `*p` | the pointee type, and the result is an lvalue |
| `a[i]` | identical to `*(a + i)` — including `i[a]`, which is legal |
| `f(...)` | the function's return type |

### 10.1 lvalues

An expression that designates an object. Only an lvalue may appear on the left
of `=`, or as the operand of `&`, `++` or `--`. Names, `*p`, `a[i]` and
`s.member` are lvalues; the result of arithmetic never is. An array is a
*non-modifiable* lvalue: `&s` is legal, `s = ...` is not.

The current compiler encodes this by accepting only a bare name to the left of
`=`. That check gets replaced by a real `isLvalue()` on the expression node.

---

## 11. What code generation must do differently

Today every value is 64 bits in `%rax` and every local is an 8-byte slot. Types
break all three assumptions.

### 11.1 Loads and stores become width-correct

| Type | Load into `%rax` | Store from `%rax` |
| --- | --- | --- |
| `char` (signed) | `movsbl m, %eax` | `movb %al, m` |
| `unsigned char` | `movzbl m, %eax` | `movb %al, m` |
| `short` | `movswl m, %eax` | `movw %ax, m` |
| `unsigned short` | `movzwl m, %eax` | `movw %ax, m` |
| `int` | `movl m, %eax` (zero-extends to 64 for free) | `movl %eax, m` |
| `long` | `movq m, %rax` | `movq %rax, m` |

**The extension choice is driven by signedness, and getting it wrong is a bug
that only appears for negative values** — which is precisely the kind of bug a
test corpus of small positive numbers will not find. Cases with negative
`char` and `short` values are mandatory.

### 11.2 Signedness changes the instruction, not just the type

| Operation | Signed | Unsigned |
| --- | --- | --- |
| divide | `cqo; idiv` | `xor %edx,%edx; div` |
| right shift | `sar` | `shr` |
| less than | `setl` | `setb` |
| greater than | `setg` | `seta` |

Four places where a type error becomes a wrong answer rather than a compile
error. `-1 / 2` and `-1 >> 1` are the cases that catch it.

### 11.3 The frame stops being uniform

Slots take the size and alignment of their type, each at an offset that is a
multiple of its own alignment. Allocating largest-first wastes the least
padding. `sizeof` on the frame is no longer `8 * count`.

### 11.4 Floating point is a separate register file

`float` and `double` live in `%xmm0`–`%xmm15`, use `movss`/`movsd`,
`addss`/`addsd`, and convert with `cvtsi2sd`, `cvttsd2si`, `cvtss2sd`. Under
System V, arguments are classified INTEGER or SSE **independently**, so the
first six integers go in the six integer registers and the first eight floating
values in `%xmm0`–`%xmm7`, counted separately.

`%al` must hold the number of vector registers used when calling a variadic
function. The compiler already emits `mov $0, %rax` before every call, which is
correct only while no floating point exists. **That instruction becomes a real
computation in stage 3**, and it is the point at which the currently unverified
stack-alignment padding finally gets a test: `printf("%f\n", 1.5)` is what makes
a misaligned `%rsp` crash.

---

## 12. Struct, union, enum

Stage 4, but the layout rules belong here.

- **Struct**: members in declaration order; each at the next offset that is a
  multiple of its alignment; the struct's alignment is the largest of its
  members'; the total size is rounded up to that alignment, so arrays of it stay
  aligned. Trailing padding is therefore part of `sizeof`.
- **Union**: every member at offset 0; size is the largest member rounded up to
  the largest alignment.
- **Enum**: a distinct type, compatible with an implementation-defined integer
  type — `int` on all three targets. Enumerators are `int` constants.
- **Bit fields**: deferred. They have their own allocation rules and are
  genuinely target-dependent; nothing in the plan needs them.

---

## 13. Diagnostics the checker must give

A type system is only worth having if it explains itself. The messages, all with
a line and a caret:

```
'x' was not declared
'f' takes 2 argument(s), given 1
cannot assign 'char *' to 'int'
cannot assign to a const object
'*' cannot be applied to 'int'
subscript on something that is not an array or pointer
too many initialisers for 'char[4]'
'long float' is not a type
'p - q' needs two pointers to the same type
```

Every one of these is a case where the current compiler would emit code and
produce a wrong answer, or crash.

---

## 14. Staging

| Stage | Contents | Ends when |
| --- | --- | --- |
| **1** — **done** | `Type`, `TypeTable`, `Target` sizes, integer types, `signed`/`unsigned`, specifier parsing, promotions and the usual arithmetic conversions, `sizeof`, casts, width-correct loads and stores, signed/unsigned instruction selection | done: 84 cases, `i < u` is 0 and `l < u` is 1, both agreeing with gcc |
| **2** — **done** | Declarator grammar, pointers, arrays, decay, string literals, globals, `static`, `extern` | done: 139 cases. `char string[16]` works, `sizeof` gives 16 outside a function and 8 for the same parameter inside |
| **3** | `float`, `double`, `long double`, SSE, the SysV classification, `%al` for variadic | `printf("%f")` works — and the alignment padding is finally proved |
| **4** | `struct`, `union`, `enum`, `typedef`, the typedef ambiguity in the parser | A linked list compiles |

Each stage keeps the suite green at every commit, as every increment so far has.

---

## 15. How it gets tested

The existing method, extended: every case compiled by `cc1` and by `gcc`, both
run, both required to agree on exit status **and** printed output.

Types need cases the current corpus has no reason to contain:

- negative `char` and `short` round-tripped through memory — catches a wrong
  extension instruction
- `unsigned` compared against `signed` at each rank — catches the usual
  arithmetic conversions
- `-1 / 2`, `-1 % 2`, `-1 >> 1` versus their unsigned counterparts
- `sizeof` of every type, and of an array both inside and outside a function
- integer overflow at each width, where the behaviour is defined
- `(char)(x)` truncation, and widening back
- struct layout probed by `sizeof` and by member offsets

`sizeof` results must be compared against gcc **on the same target**, never
against numbers written in this document. That is what stops the LP64 and LLP64
difference from becoming a silent bug when the Windows backend lands.

---

## 14a. The alignment gap, closed

Recorded here since the functions increment: the compiler pads `%rsp` to a
16-byte boundary before every call because System V requires it, and **nothing
in the suite could tell whether that padding was there**. Deleting it left
every case passing, because `putchar` never executes an instruction that
demands the alignment.

Floating point ended that. `printf` with a floating argument makes libc save
its register area with an aligned SSE store, and a misaligned `%rsp` faults on
it. With the padding removed, `fp_printf_deep` now exits **139** — SIGSEGV —
where gcc's build of the same file exits 0.

Only that one case catches it, and the reason is worth keeping: the padding is
only emitted when an odd number of values is already stacked at the call. Most
calls happen at even depth and need none. `fp_printf_deep` was written to put
the call inside a larger expression precisely so the depth would be odd.

## 15a. A coverage lesson worth keeping

Breaking pointer scaling - forcing `p + n` to move by bytes instead of
elements - left `ar_loop` passing. It sums `a[i]` for ten elements and returns
the total, and the total was garbage, but with byte-scaled indexing the low
byte of every element is still `i`. **The exit status carries only the low
byte, so the sum's bottom eight bits were still 45.**

The one-byte channel described in section 10 is not only a limit on what a
program can say; it is a limit on what a test can detect. Any case whose answer
is congruent to the right one modulo 256 passes.

Cases that measure something must compare exactly - `(a[0] == 0) * (a[9] ==
9000)` - or print, so the output comparison catches it. Returning an aggregate
is the weakest form of assertion available here. Three cases were added on that
basis and the same injection now fails five instead of two.

## 16. Open questions

Stage 1 was built on the proposals below rather than waiting on them. Each is
still open to being changed; none is buried.

1. **The dialect** — built as C89 plus the four additions in section 1.
2. **`long double`** — deferred with the rest of floating point to stage 3. No
   decision made yet on whether to give it real x87 80-bit support.
3. **Bit fields** — deferred, as proposed.
4. **`register`** — *not* accepted and ignored, in the end. It is refused with
   "'register' is not supported yet", along with `static`, `extern` and
   `const`. Ignoring `const` would let an assignment through it compile, which
   is worse than saying no, and the same reasoning covers the other three.

## 17. What stage 1 left out — since added

**`&&`, `||` and `!`** were conspicuous by their absence once the tests were
written, and are now in. They were kept out of stage 1 deliberately: they are
control flow rather than typing. Neither operand is converted to a common type,
each is only tested against zero, and the result is an `int` valued 0 or 1
whatever went in — so they bypass the usual arithmetic conversions entirely.

Short circuit is their whole point and it is observable, which made it
testable: `0 && putchar(65)` must print nothing. Removing the short circuit
leaves every exit status unchanged and is caught only by the output
comparison — `C` became `ABC`.
