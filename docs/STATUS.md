# What is implemented

Where the compiler stands. Taken from the code rather than from memory, and
meant to be re-derived the same way when it drifts — every number here is
countable from the repository.

Companion documents: [`TYPES.md`](TYPES.md) settles the type system and its
staging; [`../demo/README.md`](../demo/README.md) walks two programs from
source to assembly to answer.

---

## Scale

**8,989 lines of C++ in 24 files**, built by `g++` under
`-Wall -Wextra -Werror -pedantic -pthread`, plus **395 lines of C in 6 shipped
headers**. **397 single-file cases, 8 multi-file ones, and 1 about the driver
itself**, plus **14 for `x86_64-windows`** — run twice, through clang and through
ml64 — and **12 for `arm64-darwin`**, all
passing.

| File | Lines | Does |
| --- | --- | --- |
| `Parser.cpp` / `.h` | 2,715 | parsing, type checking **and** constant folding — C cannot separate the first two |
| `backend/X86_64Linux.cpp` / `.h` | 1,433 | x86-64, GNU as syntax — System V and Microsoft x64 out of one generator |
| `backend/Arm64Darwin.cpp` / `.h` | 1,238 | AAPCS64 as Apple builds it — a subset, and it runs |
| `Preprocessor.cpp` / `.h` | 978 | includes, conditionals and macros, before the lexer |
| `backend/Masm.cpp` / `.h` | 566 | the generator's own output, respelled for ml64 |
| `Driver.cpp` / `.h` | 543 | arguments, `-arch`, `-S`/`-c`, `-D`/`-U`, the include search path, the link step, and the jobs — one per input, on threads when there are enough |
| `Ast.h` | 493 | the node hierarchy and the visitor |
| `Type.cpp` / `.h` | 345 | types, interning, and the abstract `Target` |
| `Lexer.cpp` / `.h` | 278 | text to tokens |
| `backend/Backend.cpp` / `.h` | 185 | what a platform is, the registry `-arch` searches, and which segment an object belongs in |
| `Source.cpp` / `.h` | 108 | the text, the line map, and every diagnostic |
| `backend/X86_64Windows.cpp` / `.h` | 101 | LLP64 sizes and Microsoft x64, on the shared generator |
| `main.cpp` | 6 | nothing but a way in |

Every number in that table was wrong again when it was last read, and this time
so was the shape of it: **`Masm.cpp` had no row at all**, so 566 lines of the
compiler were absent from a table whose stated purpose is to account for all of
them, and the total was short by exactly that much. `Arm64Darwin.cpp` was the
worst of the rows, recorded at 781 against 1,238 — it grew by more than half
while the document said it had not moved.

That is the third time this table has been found wrong rather than found right,
which is the argument for deriving it instead of reading it: `wc -l` over `src/`
answers every number above, and `ls tests/cases/*.c | wc -l` the case count.
Note that `tests/cases` holds one file that is not a case — `pp_helper.h`, which
an `#include` case includes — so counting entries rather than `*.c` overstates
it by one.

**`src/backend/` holds one file per platform**, and each carries three things:
what its types measure, what its ABI decides, and the code generator when there
is one. `-arch` names which, **defaulting to the host it was built on** — the
Mac build targets `arm64-darwin`, the Linux build `x86_64-linux`. All three now
emit.

That default used to be `x86_64-linux` whatever the host, and it made the
compiler look broken at the first step on a Mac: `cc1 f.c -o f.s` wrote x86-64
for ELF, and `clang f.s` answered `unexpected token in '.section'` and then a
page of unknown registers, none of which says the target was wrong. Emitting
for a machine that cannot assemble the result is a reasonable thing to ask for
and a poor thing to assume — so it is still one flag away, `-arch
x86_64-linux`, and no longer what you get by not choosing.

The file to notice is `X86_64Windows.cpp`. It holds a table of sizes, a table
of registers, seven ABI facts and a `codegen` that hands back the same
`X86_64Linux` the Linux backend uses. That is what "the calling convention is
data" was for.

**Windows is assembled by Microsoft's own toolchain.** `cc1 -arch
x86_64-windows` writes **MASM** by default, `ml64` assembles it and `link.exe`
links it against the static CRT — a PE32+ binary with nothing in its path
borrowed from another toolchain. `-masm=gnu` still writes the GNU spelling,
which is what `tests/windows.sh` assembles with gcc on Linux.

This is a translation, not a second code generator, and the distinction is the
whole design. Every instruction cc1 selects for Windows is the instruction it
selects for Linux — the `Abi` is the whole of what differs — so a second
emitter would be the same selection written twice, and would drift. What
differs is only how an instruction is *spelled*, so `src/backend/Masm.cpp`
rewrites the generator's output: operands the other way round, no `%` or `$`
sigils, `[rbp-4]` for `-4(%rbp)`, `DWORD PTR` where GNU keeps the size in the
mnemonic's suffix, and the segments, `PROC`/`ENDP` and `EXTERN` declarations
MASM wants stated rather than inferred.

Three things it learned by being run over the corpus rather than by being
reasoned about, each of which assembles or fails on real `ml64`:

- **`movsd` needs its size named.** MASM's `movsd` is also the string-move
  instruction, so `movsd [rsp], xmm0` is ambiguous and refused; it has to be
  `movsd QWORD PTR [rsp], xmm0`.
- **MASM reserves every mnemonic and register as an identifier, and C does
  not.** The corpus has globals called `gs` and functions called `add`, `mul`,
  `sub` and `fabs` — all perfectly good C, all a syntax error to `ml64`. Such a
  name is given a `$` in front. Every unit here is compiled by cc1 and mangles
  identically, so cross-file references still meet; what it cannot survive is
  linking against an object from another compiler exporting the unmangled name,
  which is the honest cost of the assembler owning those words.
- **A long string is one `DB` in GNU and too many for one statement in MASM**,
  which answers "statement too complex" and then reports the label as
  undefined. The items are dealt out sixteen to a line.

Anything the translation does not recognise stops the compiler and names the
line. Silence would mean emitting an instruction that assembles into something
other than what was meant, which is the one failure this file must not have.
All **390** cases the Windows backend can compile assemble cleanly under
`ml64`, which is how each of the three above was found.

**`arm64-darwin` emits, and what it emits runs.** Integers, pointers, globals,
arrays, control flow, recursion, calls, **floating point, postfix `++`,
`switch` and structs** all work,
and they are checked the way the x86-64 backend is: compiled twice, run twice,
compared.

**Floating point here is one register file seen two ways.** A `float` occupies
`s0` and a `double` occupies `d0`, and they are the same register — so naming
the wrong view assembles cleanly and computes at the wrong precision, which is
why the width is asked of the type at every site rather than written down. Every
spill goes through the `d` view, which is exact for both, because any write to
the `s` view zeroes the top half.

AArch64 cannot name a floating constant in an instruction, so a literal travels
as its bit pattern through `x9` and `fmov`s across — a move of the bits, not a
conversion. `scvtf` and `ucvtf` read `x0` whatever the source width, since an
integer here is always already extended to 64 bits for its own type, so one
instruction serves `char` through `long` and the signedness picks which.

**The comparison conditions are the IEEE ones, and they are not the signed
integer ones.** After `fcmp` an unordered result sets C and V with N and Z
clear, so `lt` — which tests N≠V — calls `NaN < x` true. `mi` reads false, and
`ls` is the equivalent for `<=` where `le` is not. This is the same trap the
x86-64 backend fell into from the other direction, where `ucomis` sets ZF, PF
and CF together; there three of nine comparisons were wrong and a spot check of
one would have passed. `tests/arm64/fp_nan_ordering.c` is the x86 case copied
across unchanged, and putting `lt` back fails it while the other six cases stay
green.
`tests/arm64.sh` is that runner, and it runs **on the Mac**, because the Mac is
arm64 and can execute what this backend produces. The reference there is clang
rather than gcc — a different compiler, the same argument: it is the
implementation sitting on the same disk.

**Apple's variadic rule is the thing that caught this backend out.** AAPCS64
puts arguments in `x0`-`x7`; Apple puts the *variadic* ones on the stack in
eight-byte slots, in registers never. Following the standard printed
`n=1809625552 m=-1899641628` where `42` and `7` were meant — a failure that
looks like a wild pointer and is a calling convention. So `Call` now carries how
many arguments the prototype named, since the split between named and variadic
is invisible from the argument list alone.

**`x86_64-windows` emits too, out of the Linux generator unchanged.** A subset,
refused by name where it stops: a struct or union passed by value or returned,
and a variadic definition. What works is integers, pointers, floating point,
globals, arrays, control flow, recursion and calls — through
`%rcx %rdx %r8 %r9`, positionally, over 32 bytes of shadow space — and
`printf`, with a `%f` in it.

**A variadic float travels in both files here.** Microsoft x64 has no `%al`
convention and gives the callee no prototype, so the bits go to the vector
register *and* to the integer register of the same slot, and `printf` reads the
integer twin. Put them only in `%xmm2` and every conversion but the `%f` comes
out right. `tests/windows/w_variadic_double.c` makes the `double` the third
argument on purpose: slot two is `%r8` and `%xmm2`, so a backend still counting
the two files independently is wrong about the register as well as the pairing.

**`printf`, `sprintf` and `fprintf` agree on both targets**, which is one
function with three destinations and is worth checking as one:
`tests/cases/io_three_agree.c` and `tests/windows/w_stdio_family.c` require the
same conversions, the same returned count, and `strlen` of the buffer to match
what `printf` said it wrote. All three are variadic, so on Windows all three
rest on the both-files rule above.

**Not every one of them is a symbol on Windows**, which cost a link error to
find out. Microsoft's UCRT keeps `printf`, `fprintf`, `puts` and `fputs` as
real exports, and makes `sprintf`, the whole `v` family and the `scanf` family
**inline wrappers** over `__stdio_common_*` in its own `<stdio.h>`. A compiler
that declares them as the ordinary functions C says they are — which cc1 does,
correctly — has nothing to link against. `tests/windows-native.sh` therefore
links `legacy_stdio_definitions`, which is the library Microsoft ships for
exactly this case. It is not a workaround and not a cc1 defect; it is what that
platform requires of any compiler that does not inline its own stdio.

That case is marked `// windows-only:`, which is new. It calls the C library,
and a Windows-convention call into glibc's System V `printf` is precisely the
boundary the Linux-hosted suite depends on nothing crossing — it segfaults. So
that suite compiles it, applies the `// forbid:` checks, and leaves running it
to `tests/windows-native.sh`, where it means something.

**What it emits runs on Windows.** `tests/windows-native.sh` runs from the Mac,
compiles each case there, relays the assembly to a Windows 11 machine on the
LAN, and has `clang` assemble it — AT&T syntax, PE/COFF, linked against the real
C runtime — and run it. All 12 pass, `printf` included. `cl` and `ml64` are on
that machine too and neither can do this job: `cl` compiles C, `ml64` reads
MASM, and cc1 writes GNU syntax.

**Microsoft's own compiler confirms the two rules that matter.** That is a
stronger statement than the suite passing, because it does not depend on cc1
being right at both ends of a call. `cl /FAs` on `probe(int a, double b, int c,
double d)` puts `b` in `xmm1`, `c` in `r8` and `d` in `xmm3` — the positional
rule, and not one of the three is where System V would put it. For six integer
arguments its listing reads `a$ = 8` through `d$ = 32`, which are the four
shadow slots, and then `e$ = 40`: the fifth argument sits immediately above the
shadow area, exactly the `16 + shadowBytes` the backend computes.

**It is also checked on Linux, which sounds like a contradiction and is not.**
The build box cannot reach the Windows machine — it is in AWS and that is on a
LAN — so `make test` needs an answer that works without one, and it has one. A
Windows-convention program that makes no library calls is a self-contained blob,
and the only boundary is `main` itself, which takes no arguments and returns
`int` in `%eax` under both conventions, so glibc calling it cannot tell. Where
argument three lives, where the fifth one sits, who opens the shadow space: all
of that is between functions `cc1` emitted, and Linux never sees it.

**The whole Linux corpus was put through it once, as a sweep rather than as a
suite.** Of the 379 single-file cases there were on the day — none written with
Windows in mind — the
backend accepts 359 and refuses 20 by name: 19 for the aggregate and variadic
rules above, and one for something better. `bf_types.c` declares
`unsigned long l : 40`, which is legal where `long` is 64 bits and impossible
where it is 32, and the front end says so with the caret on the field. Same
source, same compiler, two correct answers.

Of those 359, **349 give the same answer on Windows that they give on Linux**,
and the 10 that do not are all explained:

- **Nine** are `sizeof(long)`. Every one mentions `long`, and each is right on
  both platforms — `ce_conditional_const.c` computes
  `sizeof(long) == 8 ? 8 : 4` and returns 0 on Windows where the case expects
  the 8 it gets on Linux. The expectation is Linux's, not the compiler's.
- **One** is the C library rather than the compiler. `file_streams.c` uses
  `stdout` as an `extern FILE *`, which is what it is on glibc; the Windows
  UCRT makes it a macro over `__acrt_iob_func(1)`, so there is no such symbol
  to link against.

The sweep compared exit status only, not printed output, and it is not wired
into `make test` — the corpus is the *Linux* backend's, its expectations are
Linux's, and nine of them are supposed to differ. `tests/windows/` is the
Windows corpus. This was a measurement, and what it measured is that the
divergence between the two targets is entirely the data model.

**The Linux suite is the stronger of the two on one rule**, which is why both
are kept rather than the older one being retired. Microsoft x64 makes `%rdi` and
`%rsi` the callee's to give back where System V makes them scratch, so the
generator's scratch here is `%r10`. Putting `%rdi` back leaves all 9 passing *on
real Windows* — the C runtime happens to hold nothing in `%rdi` across its call
to `main`, so nothing notices. The `// forbid:` line in `w_callee_saved.c`
catches it every time, by reading the assembly. A binary that runs is evidence,
not proof.

**The suite would otherwise be marking its own work.** `cc1` is on both ends of
every call in it, so a rule it has wrong it has wrong symmetrically and the
answer still comes out right. Two cases exist to break that symmetry, and their
callers — `tests/windows/msabi_slots.S` and `msabi_stack_args.S` — are written
by hand from the Microsoft specification rather than from this compiler.

The three rules were each injected to prove the suite sees them. Counting the
register files independently, System V's way, breaks `msabi_slots`: for
`probe(int, double, int, double)` the two conventions disagree about all four
arguments, so it returned 177 for 18. Using `%rdi` as scratch trips the `grep`.

**The shadow space was not caught, and that is why there are two hand-written
callers rather than one.** Setting `shadowBytes` to zero left the whole suite
passing — a caller reserving nothing and a callee expecting nothing agree
perfectly, and the one hand-written caller at the time passed only four
arguments and never touched the stack. `msabi_stack_args.S` passes six. With it
the same injection returns 102 for 91.

**The compiler predefines its own macros now**, which it did not before: not
even `__STDC__`. `__FILE__` and `__LINE__` already worked and are a different
kind of thing — they answer *where am I*, change at every use, and are
special-cased in the expander rather than held in a table. What was missing was
the other kind, *what am I being compiled for*, which is constant for a whole
run and belongs in the table.

About a dozen, against gcc's 368: `__STDC__`, `__STDC_HOSTED__`, `__CHAR_BIT__`,
the widths taken from `Target` (`__SIZEOF_LONG__` and the rest), and each
backend's own names — `__linux__` and `__ELF__` and `__LP64__`, or `_WIN32` and
`_WIN64`, or `__APPLE__` and `__aarch64__`. The widths are derived rather than
written down, so a target cannot claim a `long` it does not have. They are
seeded as ordinary object-like macros, so `#undef` and `#ifdef` reach them
exactly as they reach a `#define` in the file.

**A variadic function can now be written in the language, not only called.**
`va_start` is `__builtin_va_start` in the grammar rather than a macro, because
which parameters were named is a property of the definition and the front end
already knows it. `lib/stdarg.h` supplies `va_list`, and the three `<stdio.h>`
functions that take one — `vprintf`, `vfprintf`, `vsprintf` — are declared
there rather than in `stdio.h`, so the name `va_list` reaches only the files
that asked for it. Those three were listed as absent for exactly this reason
until now.

**The layout of `va_list` is not this compiler's to choose.** `vprintf` is in
the C library and reads whatever System V says a `va_list` is, so the record in
`stdarg.h` matches glibc byte for byte or the first forwarded call walks the
wrong memory. It is declared as an array of one, which is what makes passing it
hand over a reference rather than a copy — and what makes `__builtin_va_start`
receive an address without a `&`.

The prologue of a variadic function spills its six integer registers and eight
vector ones into 176 bytes of frame, and `va_start` records where. The vector
half sits behind `testb %al, %al`, the count the caller left: a caller that
passed no floating point may have left rubbish in `%xmm0`-`%xmm7`, and spilling
regardless is how that becomes a fault. `tests/cases/vd_forward.c` passes a
`double` for that reason — a prologue that saved only the integer half prints
the strings and the integers correctly and gets only that one conversion wrong.

**This is the first feature where Linux is the hard target, and Windows now
proves it.** System V hands a variadic callee its arguments in the ordinary
registers, so the callee must build somewhere addressable to walk: 176 bytes,
fourteen spills and an `%al` guard. Microsoft x64 needs none of it. Every
argument, named or not, owns a consecutive eight-byte slot from `16(%rbp)` up,
and the first four of those *are* the shadow space the caller already left — so
the callee spills `%rcx %rdx %r8 %r9` into a place that exists, and `va_list` is
one pointer at the slot after the named ones. Four instructions against
fourteen, for the same capability.

`stdarg.h` branches on `_WIN32` to say so, which is what the predefined macros
were for. The two `va_start` macros differ in one respect only: System V's
`va_list` is an array of one and decays to its own address, and Windows's is a
`char *` that has to be given one — so the builtin receives a pointer to the
`va_list` either way.

`arm64-darwin` still refuses `va_start` by name. Apple puts the variadic part on
the stack, so its `va_list` is a pointer too and the work is the same size as
Windows's; that backend has larger gaps to close first.

`va_arg` is not written. Forwarding to the C library's `v` functions does not
need it, which is why this stops here: under System V it is a branch on the
argument class, a bounds check against the offset for that file, and a fall
through to the overflow area — code with control flow in it, rather than four
stores.

**The calling convention is data rather than code.** x86-64 Linux and x86-64
Windows share every instruction this compiler emits, down to the mnemonics —
what separates them is entirely the convention, so there is one generator that
consults an `Abi` rather than two files that are ninety per cent the same:

| | System V | Microsoft x64 | AAPCS64 (Apple) |
| --- | --- | --- | --- |
| Integer argument registers | 6 | **4** | 8 |
| SSE / vector registers | 8 | **4** | 8 |
| How the two files are counted | independently | **positional** | independently |
| Caller shadow space | none | **32 bytes** | none |
| Struct returned in registers up to | 16 bytes | **8 bytes** | 16 bytes |
| An oversized aggregate travels | copied to the stack | **by reference** | by reference |
| Variadic SSE count in `%al` | yes | no | no |

**The positional row is the one that makes a liar of this document.** Further up,
under *Structs by value*, it says the two register files "run out independently,
so a call can cross the boundary in one lane while the other still has room."
That is true, and it is System V, and it is written as though it were a fact
about calling conventions. Under Microsoft x64 the *n*th argument takes the
*n*th slot in whichever file it comes from, and spending one spends the other.
A second convention is what turns a sentence like that from documentation into
a bug, which is the argument for standing the other two up before writing a
single instruction for them.

Two pieces of System V had leaked out of the backend and moved back with it.
`classifyEightbytes` — the eightbyte classification, which no other ABI has —
was declared in `Type.h` beside a type model that is meant to be
platform-neutral. And the parser tested `size > 16` to decide whether a struct
comes back through a hidden pointer, which is a System V number: Windows x64
says 8. The parser still has to know the answer, because only the parser can
reserve the caller's frame slot, but it is no longer the one deciding it.

**The source carries 129 lines of marginal comment**, counted as indented `//`
lines inside `src/` — `grep -c '^ \+//'` answers it, and 37 more sit at column
zero as file and section headers. That is deliberate rather than neglected:
1,564 lines of comment were removed in one pass, which is where the total fell
from 7,207 — the code did not shrink, the prose beside it did.

This paragraph said **nineteen** for a long time and was the worst number in the
document, being wrong by a factor of six rather than by a row's worth of drift.
Nineteen was true of the ten places named below when they were counted; what it
missed is that the rule was applied to everything written since, and each new
one landed without the total being counted again. A claim about a total has to
be re-derived from the total.

What is written back is held to one test: a comment earns its place by marking
somewhere the right code and the wrong code look alike. Nineteen lines across
ten places, and five of the ten are injections the suite catches — the call
padding counted before the pushes rather than after, the reverse order of those
pushes, `%r11` rather than `%rax` for an indirect call, and the two argument
counts that have to shift together when a return travels through a hidden
pointer. Break any of those and a program still compiles and still runs.

The other five are not injections, and not decoration either. Macro arguments
are expanded before the macro is marked busy, without which `MAX(MAX(1,9),2)`
leaves the inner call standing. The parenthesised declarator is read twice,
because it cannot be read left to right at all. A slot saves the caller's return
pointer, and the note is there to say why one is needed. A struct-valued call or
`?:` has a frame slot to read a member through without being an lvalue. And an
`extern` local travels the route a `static` local already had. Each marks either
a mistake that was actually made, or a line that reads as arbitrary until
someone says why.

Everything else that used to sit in the margins is in the commit that introduced
it and in this document. That is the trade: `git log` and `git blame` answer
*why* and the file answers *what*, rather than both answering both and drifting
apart — which they had, and this table is the proof. The `Lexer` row read 262
for several commits while the files held 310, because a row is easy to leave
behind and a total is not. Every number here is countable, and worth counting
again when it looks wrong.

**`cc1 hello.c` now produces a program**, named by `-o` or `a.out`. The compiler
still emits nothing but assembly — what changed is that the driver finishes the
job, writing the assembly to a temporary file and handing it to the host's `cc`
to assemble and link. `CC1_CC` names a different one.

`-S` stops where the whole thing used to stop, and writes the assembly instead:
one `.s` per input, or `-o` to name the output of a single one, or standard
output when there is one input and no `-o`. Every suite and tool in the tree
passes `-S`, because what they compare is the compiler and not the assembler.

That flag arrived late, and the reason it had not existed is worth keeping.
Emitting assembly only kept the surface under test to the part being written,
and it was the right default while there was one target and it was not the host.
It stopped being right when a Mac build started emitting code the Mac could run:
`cc1 f.c -o f.s` and then `clang f.s` is two steps that a C compiler is expected
to take for you, and the second one is where a wrong `-arch` announces itself
with a page of unknown registers instead of a sentence.

**Several inputs link together**, which is separate compilation reaching the
driver at last: `cc1 main.c helper.c -o prog` compiles each — on threads, when
there are enough of them — and links the results. Under `-S` the old rule still
holds and `-o` with several inputs is refused, since that names one file.

**`-c` stops in between**, at one object per input, named by `-o` or after the
input in the current directory, which is what `cc` does. And **`-D` and `-U`
reach the preprocessor's macro table**, the same table the target's own names
are seeded into — so `-DN` is `-DN=1` as everywhere else, and `-U__linux__`
takes one of the backend's own macros back off. Both were missing entirely,
which is a larger hole than it sounds: no non-trivial C project builds without
`-D`, and the driver could predefine a dozen macros while refusing to accept
one.

**Linking only works for the host.** Cross-compiling is still available and still
useful, and it now says where it stops rather than handing the assembler
something it cannot read:

```
cc1: cannot assemble and link x86_64-linux code on this machine, which is
  arm64-darwin - use -S to write the assembly and take it there
```

**Every** message the driver can produce — the usage line, the unknown-option
refusal, `-o` with no name, `-o` against several inputs, `-j` with no number,
and a file it cannot write — names `argv[0]` rather than the literal string
`cc1`, echoed verbatim rather than trimmed to a basename. Of the five that
existed when this started, two were converted and three were quietly left
saying `cc1:`; all five are converted now, and the `-j` messages were written
that way from the start. Hence *every*, and hence counting them. A compiler built by hand under another name
is the ordinary case, and one built as `cpp` shares its name with the system
preprocessor on the `PATH` — which accepts the same `-o`, writes something
plausible and exits 0. A diagnostic reading `cc1:` when the user typed `./cpp`
removes the last signal that these are two different programs. Trimming to the
basename would make two binaries print the same word again, which is the thing
being avoided.

Several inputs in one invocation are compiled **on threads**, four or more by
default, `-j n` to name the count and `-j 1` to force the serial loop. The
threads share one index rather than taking a slice each, because the jobs are
not equal — the largest test program costs twenty times the smallest, and fixed
shares would leave one thread holding the long ones.

**How many threads is asked of the machine on every run, and the question is how
many _cores_**, counted as distinct `(package, core)` pairs over the CPUs this
process is allowed on. `std::thread::hardware_concurrency()` answers with
logical CPUs, and the difference decides the behaviour here: this box reports
two and has one physical core with two SMT siblings, so the automatic answer is
one thread and the serial loop. An explicit `-j n` is taken as asked, because
someone naming a number knows something the driver does not.

The measurement that settled it. Compiling 361 test programs, and then a
generated corpus of 12 files and 432 013 lines where per-file work dominates:

| | |
| --- | --- |
| 361 files, one invocation | **0.02 s** |
| 361 files, 361 separate invocations | **0.61 s** |
| 432 k lines, `-j 1` | **2.01 s** |
| 432 k lines, `-j 2` | **1.99 s** |
| 432 k lines, `gcc -O0 -S` file by file | **24.38 s** |

Two things fall out. **Starting the process costs thirty times what compiling in
it does.** And **the threads buy one per cent** — which is not the loop failing
but this machine's ceiling: two pure integer loops take 1.57 s against 0.77 s
for one, so SMT siblings give ALU-bound work almost nothing, and two separate
processes collect the same one per cent. The threads are worth having for the
shape the compiler will need when it grows a middle end, not for the
milliseconds. See [`PARALLELISM.md`](PARALLELISM.md) for the whole picture.

One thing threads change that is not speed: a diagnostic ends the process, so
the first failure wins and the jobs still running are abandoned wherever they
had reached, which may be halfway through writing an `.s`. That file is garbage
either way, because the compilation that owned it did not finish.

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

### Pointers to functions

`int (*f)(int, int)` — declared, assigned, reseated, passed as a parameter, held
in an array, compared, and measured by `sizeof`. `qsort` and `bsearch` are
declared in `<stdlib.h>` because of it, and they are the argument for the
feature: a comparison function cannot be spelled without this type.

**A function type is what it returns and what it takes**, and it exists only to
be pointed at — C has no object of function type. So `Kind::Function` reuses
`pointee_` for the return type, holds its parameter list, and is interned
structurally: two spellings of `int (*)(int)` reached from different
declarations must be one type, or assignment and argument checking, which decide
compatibility by pointer equality, would call them different.

**A function's name used as a value is a pointer to it, with no `&` written.** C
converts on its own, which is why `qsort(a, n, s, cmp)` looks the way it does.
The node built for it is the address of the function's symbol — exactly what `&`
on a global already generates, so nothing new reached code generation for that
half.

The call is one rule rather than three. `f(1)`, `table[i](1)` and any other
postfix chain ending in a pointer to a function are all handled where the
postfix loop sees a `(`, and a call by name is the only special case. Both go
through one `finishCall`, so the argument rules cannot drift apart between them.

**The address is held in `%r11` across the argument sequence.** It is evaluated
first and pushed beneath the arguments, so it comes back after every argument
register is filled. `%r11` because it is caller-saved and not one of the six
System V uses — and `%rax` in particular could not be it: `%rax` carries the
count of vector registers for a variadic callee and is written immediately
before the call. Trying it there segfaults, which is the injection that proves
the choice.

**`(*f)(x)`, the older spelling, works too**, and it was refused for a reason
that turned out to be a misreading. Dereferencing a function pointer does yield
a function type, and this model has no lvalue of one — but C never asks for one:
a function designator converts straight back to a pointer everywhere a value is
wanted. So `*` on a pointer to a function is the identity, `(*f)(x)` and `f(x)`
are one expression, and `(**f)(x)` is too. The old refusal came out of the
return-type check with a message about `int (int)`, which named the parser's
confusion rather than any rule the program had broken.

### Structs by value

A struct of **16 bytes or less** is passed and returned in registers, as System
V says: the object is split into eightbytes, each one SSE if everything
overlapping it is `float` or `double` and INTEGER otherwise, and each spends a
register from that file. So `struct { int; int; }` travels in one integer
register, `struct { double; double; }` in two SSE ones, and `struct { int;
double; }` in one of each.

Two places needed something new. A **returned** struct arrives in registers
while every other expression here reaches a struct by its address, so a call
returning one is given a slot in the caller's frame — allocated by the parser,
since only the parser knows the frame — and the registers are written there
before the address becomes the expression's value. And a **parameter** arrives
in registers and is reassembled into its own slot in the prologue, for the same
reason from the other side.

The tail of a struct that does not fill an eightbyte is moved at its real width.
Reading eight bytes would be reading past the object, and the bits above it are
unspecified anyway.

**A struct larger than that is MEMORY class and goes on the stack**, copied
there by the caller — which is the same mechanism as an argument past the sixth
register, and why both stopped being refused together.

**Arguments past the registers.** Six integer registers and eight SSE ones, and
what does not fit is laid out in memory, upwards from the callee's `16(%rbp)` —
past the saved `%rbp` and the return address. The caller pushes them in reverse,
because `push` moves downwards and the first of them has to end up lowest. **In
System V** the two files run out independently, so a call can cross the boundary
in one lane while the other still has room, and an aggregate goes whole or not
at all — System V never splits one between registers and memory. Every number in
this paragraph is that convention's rather than x86-64's, and the `Abi` table
under *Scale* is where the other two say something different.

The alignment is the part that has to be got right first rather than last: the
stack must be sixteen-byte aligned at the `call`, and the memory arguments are
part of what is sitting on it, so the padding is decided with them counted
before any of it is pushed.

**Returning one over 16 bytes** is the mirror of that, and the only place in
this ABI where an argument register is spent on something the program never
wrote. The caller allocates the result slot in its own frame — the parser does
it, since only the parser knows the frame — and passes its address in `%rdi`
before any real argument is placed. The callee saves that pointer to a slot of
its own in the prologue, because `%rdi` will not survive the body, copies the
returned object through it, and hands the same pointer back in `%rax`, which
System V requires even though the caller already knows it.

**Every integer argument therefore shifts along by one**, and both sides have to
shift together. A function of six integer parameters that returned `int` put all
six in registers; returning a 32-byte struct pushes the sixth onto the stack,
with no change to how it was written. That is one count in two places — the
caller's and the callee's — and getting either wrong leaves a program that still
runs. Both are injections, and the suite fails on each.

A struct of 16 bytes or less is untouched by this: it still comes back in
`%rax`/`%rdx` or `%xmm0`/`%xmm1`, and no register is spent on a pointer.

`f(x).m` works as a consequence rather than as a feature. A call returning a
struct already has an address — the slot — so member access off a call needed
only for `genAddr` to say so. It had been refused for every struct size, not
just the large ones, and the refusal came from code generation with no line
number. The object is still not an lvalue, and nothing here makes it one.

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

**Which object the qualifier lands on is decided by the declarator**, and that
is the part this compiler had backwards. A qualifier before the `*` belongs to
the pointee and one after it belongs to the pointer, so `const char *s` leaves
`s` itself assignable and `char *const s` does not. What happened instead was
that the `const` from the specifiers was applied to the declared object whatever
the declarator said — so `s = s + 1` on a `const char *` was refused outright,
and `char *const` was accepted and then allowed to be reassigned. Both are
wrong, and the first is the one that matters: walking a string is what a
`const char *` is *for*, and every loop of that shape was rejected.

`qs_const_param.c` had covered this since it was written and passed throughout,
because it only reads `s[0]`. A parameter that is never advanced cannot tell the
two readings apart.

**What is still not caught is worth stating separately.** The pointee's
qualifier is parsed and then dropped, so `const char *s` does not make `*s`
read-only — `s[0] = 'x'` is accepted. Carrying it would mean making
`const char *` a distinct interned type from `char *`, which reaches every
comparison in the parser, since assignment, calls and `?:` all decide
compatibility by pointer equality on interned types. That is a missing check.
The object's own qualifier, which is what the declarator decides, is now
enforced on locals, parameters and globals alike.

`volatile` is accepted and changes nothing, and that is honest rather than lazy:
this is a stack machine with no register allocator, so every value is written to
memory and read back on each access already. There is no caching for `volatile`
to forbid. It would start to mean something the day values live in registers
across statements.

`auto` and `register` are accepted on a local and ignored for the same
reason, and it is the clearer case of the two: it asks for exactly the thing
this compiler has decided not to do. A hint that cannot be taken is still a
declaration that has to parse, and refusing it turned ordinary C away over an
optimisation nobody was going to get.

**But it is not ignored entirely, because `register` is the one qualifier here
with a rule attached.** A register object has no address: `&k` is refused, on a
local and on a parameter alike, exactly as it is on a bit-field and through the
same idea — a name this compiler will not let you point at. That refusal is the
whole observable content of the keyword, and it is the part that would have been
easy to leave out, since accepting-and-ignoring passes every test that does not
try to take an address. `register` at file scope is refused too; it is a storage
class for a local or a parameter and there is nothing there for it to qualify.
`auto` is refused there too, and on a parameter, where C allows only `register`.

**All 32 of ANSI C's keywords are now accepted**, and that number is checked by
compiling one program per keyword rather than by reading the parser. `auto` was
the last, and it had been failing with `'auto' was not declared` — a message
that blames the program for a gap in the compiler, which is the failure mode
worth naming. `atDeclarationStart` had never listed it, so `auto int x;` was
read as a statement beginning with an unknown name.

**A `typedef` inside a block is still not accepted**, and the attempt to add it
in the same breath is why it is called out here rather than quietly left. That
list was missing `typedef` too, and adding it made most block-scope typedefs
work — and made one case **hang the compiler**: a nested block redeclaring a
typedef name already in scope spins instead of reporting the duplicate, because
the typedef table is one flat map with no scope to it, so the inner declaration
is not a new binding and the loop that should have ended does not. A hang is
worse than a refusal, so the one line was taken back out and block-scope
`typedef` remains where it was. The table wants scopes before that keyword moves.

Locals, parameters, and file-scope objects. `static` gives internal linkage;
`extern` declares an object defined in another unit and emits nothing. Globals
may take an integer constant initialiser.

**`extern` works inside a block as well as at file scope**, naming an object
defined elsewhere without reserving a frame slot or emitting anything. It needed
no new mechanism: a `static` local already reaches a data-section symbol through
a name held beside the local, and an `extern` local is that with the plain name
instead of the decorated one.

**A *function* may be declared in a block too**, which that sentence used to
claim and did not deliver: it was true of objects only, and `extern int f(char *);`
inside a block stopped at `expected ';'`. The declarator returned the name and
then nothing knew what to do with the `(` that followed. It reaches the same
table a file-scope prototype does and emits nothing; the block limits where the
name can be seen and nothing else, since C gives such a declaration external
linkage wherever it is written — with or without the keyword. `static` there is
refused by name, being the one storage class that cannot mean anything on it. Two mistakes are caught where they are written —
an initialiser, which belongs to the definition and not to the declaration, and
a type that contradicts a file-scope declaration of the same name, which would
otherwise be found by the linker or by nobody.

A prototype may name only types — `int printf(char *, ...);` — which is how a
header is written, and now that `#include` exists it is how the files this
compiler reads will be written too. A definition may not: a body cannot use what
it cannot name, and that is refused by name. An array parameter may leave its
length out, `char s[]`, since it is a pointer either way; only the outermost
dimension may go, because the others are what decide how far one step moves.

**A prototype is mandatory.** An undeclared name is refused rather than assumed
to return `int`, and every call is checked against its signature for **the
number of arguments** and the return type.

**And the type of each argument**, against C's constraints on simple assignment
— which is what C means by saying an argument converts *as if by assignment*.
The rule is written once, in `checkAssignable`, and **all three places that
convert that way use it**: an argument against its parameter, `=` and the
initialiser written with a declaration, and `return` against the function's
type. One rule, three doorways — `char *p = 5;`, `p = 5;` and `return p;` from
an `int` function are the same mistake and now get the same message.

What passes: arithmetic to arithmetic in any direction, since every such
conversion is defined; the same type, settled by one pointer comparison because
types are interned; `void *` either way, which is the whole reason `malloc` and
`free` need no special knowledge; and the constant `0` to any pointer, which is
what `NULL` expands to. What does not pass is listed under *Not implemented*
below.

Past a variadic's named parameters nothing is checked, because there is nothing
to check against — the prototype stopped describing the arguments at the `...`,
and those take the default argument promotions instead.

This was missing until a deliberately transposed `fgets` prototype in
`<stdio.h>` — `(char *, FILE *, int)` — went uncaught, because transposing types
changes nothing at the ABI when both travel in integer registers. The same
injection now fails at the call with the argument's number in the message. A definition that contradicts
its own prototype is refused, as is a function defined twice.

### Initialisers

`int a[3] = {1,2,3}`, `struct P p = {1,2}`, and both nested in each other:
arrays of structs, structs holding arrays, two-dimensional arrays, and a `union`
taking its first member — which is all C90 offers without designators.

**A short list zeroes what it does not reach**, which is why `struct P p = {0};`
is the idiom it is. `char s[8] = "abc"` copies the characters and then zeroes to
the end. An array with no length takes one from its initialiser: `int a[] =
{1,2,3}` is three, `char s[] = "hello"` is six. Braces round a scalar —
`int x = {5}` — mean what they say.

**The same initialiser goes two different ways depending on storage duration.**
A local becomes statements, one store per scalar, built by walking a path from
the object down to each piece and rebuilding that piece's lvalue from the name
each time — rebuilt rather than cloned, because a clone of an arbitrary
expression is a thing this compiler does not have. A file-scope object or a
`static` local becomes data instead: a list of `(offset, size, value)` pieces in
offset order, with every gap between them emitted as zeroes, which covers
padding and unreached elements by the same mechanism.

**Which segment that data lands in is a separate question, and it has four
answers.** `segmentFor` in `backend/Backend.cpp` decides, once, for all three
platforms — a const-qualified object is read-only whatever its value, so
`const int z = 0;` is `.rodata` and not `.bss`, which is writable; after that an
object with no initialiser and an object initialised entirely to zero are the
same thing, and both go where the loader makes the zeroes rather than the file
carrying them.

| | ELF | Mach-O | MASM |
| --- | --- | --- | --- |
| code | `.text` | `__TEXT,__text` | `.CODE` |
| read-only | `.rodata` | `__TEXT,__const` | `.CONST` |
| writable | `.data` | `__DATA,__data` | `.DATA` |
| zero-filled | `.bss` | `.zerofill __DATA,__bss` | `.DATA?` |

Mach-O has a fifth, `__TEXT,__cstring`, because that format gives string
literals a section of their own; ELF and MASM keep them with the constants.

This used to be two segments — code and data — with everything zero written out
as zeroes. `char big[65536];` cost sixty-five kilobytes in every object file and
every binary that linked it. It now costs a number in a section header. The ELF
path also emits the `.type` and `.size` directives it had always omitted:
neither changes a byte of code, and without them `nm` reports a symbol with no
size and a debugger cannot print the object from its name.

The cost of the first is worth stating rather than hiding: an element is a
store, so `char buf[1024] = {0}` is a thousand stores where a `memset` would do.
Correct, and slow in a way a later pass could fix by noticing a run of zeroes.

A floating constant at file scope is laid out as its bit pattern, since there is
nowhere to compute one before the program runs — which is what lets a `struct`
with a `double` member be initialised there at all.

Refused by name: a bit-field at file scope, whose value would have to be packed
into a storage unit shared with its neighbours while the pieces here are whole
bytes. Assign to it in a function instead.

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
| Compound | `+= -= *= /= %= &= \|= ^= <<= >>=`, and `++` / `--` in both positions |
| Comma | `a, b` — evaluates `a` for its effects, discards it, and takes `b` |
| Conditional | `c ? a : b`, evaluating one arm, both brought to one type by the usual arithmetic conversions — so `n ? 1 : 2.5` is a `double` even when the `int` arm is taken; a `struct` or `union` too, when both arms are the same type |
| Other | function calls, `sizeof` on a type or an expression, casts |
| Literals | decimal, hex and octal integers with `u`/`l` suffixes, up to `ULONG_MAX`; `1.5`, `1.5f`; `'a'` (an `int`); `"text"` (a `char[N+1]`); every C escape including `\101` and `\x41` |

**A struct or union in `?:` needed no code generation at all**, which is the
whole story of that refusal. An arm of struct type already leaves an address in
`%rax`, because that is what every struct expression here yields, and the
conditional never looks at the type of what its arms produce. So the feature was
a refusal standing in front of working code. The rule about the arms was already
right too: they must be the same type, and since types are interned that is one
pointer comparison, so `struct A` against `struct B` is still refused by name
even when the two are laid out identically.

**What that exposed is where "has no address" was being decided.** `(c ? a : b).m`
and `f(x).m` both used to fail from code generation with no line number, because
`genAddr` had no case for either. Both do have an address — the arm, and the
call's result slot — so both now work, and the two expressions that are *not*
lvalues are refused in the parser instead, where there is a position to point at:
`&(c ? a : b)` and `&f(x)`. A member of a thing is not the same request as the
address of it, and the old code could not tell them apart.

**`x++` and `x--` are a node rather than a lowering, and the reason is worth
keeping.** `(x += 1) - 1` is the obvious way to build them out of what already
existed, and it is wrong wherever the type wraps: an `unsigned char` at 255
yields 255 and stores 0, while that rewrite computes 0 − 1. A three-bit field at
7 is the same mistake with different numbers. So the old value goes on the stack
beneath the address and comes back after the store — three things in flight,
two of them on the stack.

Refused by name: postfix on a bit-field, which would need the old value
extracted before a read-modify-write puts the new one back. The prefix form
works there, and so does `f.a = f.a + 1`.

### Statements

`return`, `if`/`else`, `while`, `do`/`while`, `for`, `break`, `continue`,
blocks, expression statements, and the empty statement.

**`return` with no value**, which is how a void function leaves early. It did
not parse at all until recently: `return` always read an expression, so a void
function could be written only if it never returned before its closing brace.
None of the 394 cases passing at the time had a bare `return` in one, which is
how it survived — the corpus is what was written, and nobody had written that.
A `return` with no value in a function that returns something is refused by
name, which is stricter than C90 and the same deliberate strictness that makes
a prototype mandatory here.

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

**All three forms of `#include`.** C90 6.8.2 defines a third beyond the two
spellings: pp-tokens matching neither are macro-expanded and read again, so
`#define WHICH <stdio.h>` and then `#include WHICH` is ordinary C and not a
malformed directive. cc1 had rejected it as one. It is how a header name is kept
in a single place and included from several, and the expansion runs before the
two spellings are looked for rather than after.

Both spellings, and the difference between them is the whole
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

`lib/` holds `stddef.h`, `stdio.h`, `stdlib.h` and `string.h` — 220 lines of
ordinary C, found through the search path baked in at build time from
`$(CURDIR)`, so a clone built elsewhere finds its own and not this one's.

**`lib/` and not `include/`, because none of it is the language.** The compiler
is `src/`. What it ships beside itself is a library a program may ignore,
replace with `-I`, or never reach for — and a file handle is the clearest case
of that: `FILE` is a struct libc defines and this compiler has no opinion about.

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

`FILE` and the file functions are here, text and binary alike: `fopen`,
`fclose`, `fprintf`, `fgets`, `fgetc`, `fputc`, `fputs`, `fread`, `fwrite`,
`fseek`, `ftell`, `rewind`, `feof`, `ferror`, `remove`, and `stdin`, `stdout`
and `stderr` as `extern FILE *`.

**This header said for a while that `FILE` was absent because an opaque handle
needs an incomplete type this compiler does not have. Both halves were wrong.**
`typedef struct _IO_FILE FILE;` — a struct never defined, reached only through a
pointer — has always been accepted, so the file layer needed no compiler work at
all and was missing only twenty lines of header text. The binary case is the one
that proves it: a `struct { int; double; char[8] }` written with `fwrite` and
read back with `fread` recovers its fields, and `sizeof` is 24 under both this
compiler and gcc, which is glibc's own layout.

Three functions are still absent, each for a reason. `vprintf`, `vfprintf` and
`vsprintf` take a `va_list`, and building one needs `va_start`, which needs a
variadic function definition — refused by name. `fgetpos` and `fsetpos` need the
caller to declare an `fpos_t`, which needs its definition, which is the kind of
thing this header exists to avoid; `fseek` and `ftell` do the same work with a
`long`.

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

Definitions with any number of parameters, recursion and mutual recursion. Calls into
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
va_start is only allowed in a function declared with '...'
```

**That list was two items long and wrong**, which is worth recording because
this document's standard is that every claim in it can be re-derived from the
repository. It was not re-derived; it was remembered. Running about fifty small
C90 programs through `cc1`, and checking every refusal against
`gcc -std=c90 -pedantic`, found these — all of them valid C90 that this
compiler does not accept:

| | `cc1` says |
| --- | --- |
| `"ab" "cd"` — adjacent string literals | `expected ';'` |
| `typedef int F(void);` — a typedef'd function type | `expected ';'` |
| `int (*get(void))(void)` — a function returning a function pointer | `expected ')'` |
| `extern int a[];` completed later by `int a[3] = {…}` | `already declared as 'int [-1]'` |
| `L'A'` — a wide character constant | `'L' was not declared` |
| `L"hi"` — a wide string literal | `'L' was not declared` |
| `#line 100 "elsewhere.c"` | `unknown directive '#line'` |
| `va_arg` | not written |
| `int *p = &g;` at file scope — an address constant | `expected a constant initialiser, and this is not an integer constant` |
| `a[i++] += 1;` — a compound assignment whose target has an effect in it | `the left of a compound assignment is read and then written, so it is evaluated twice…` |

**The compound assignment is the narrowest of them, and used to be the widest.**
`x op= e` is rewritten as `x = x op e`, which needs a second copy of the target,
so a target is only acceptable if evaluating it twice cannot be observed. The
clone used to reach through a `*` over a bare name and nothing else — and since
`x[i]` is `*(x + i)`, that meant *every* subscripted compound assignment was
refused: `x[i] += A[i][j] * b[j]` and `a[k][j] -= r * a[i][j]`, which is to say
matrix-vector accumulation and Gaussian elimination, the two loops most likely
to be the reason someone reached for a C compiler at all. The clone now copies
any target built from operands — subscripts, casts, arithmetic, member
selection — and refuses only the ones that *do* something: a call, an
assignment, a `++`, a comma, a `?:`. `a[i++] += 1` is what is left, and it is a
much rarer program than the one that was being turned away.

**The address constant** was found the same way the
rest were: by writing a program from the standard rather than by reading the
code. C90 6.5.7 allows the address of a static object as a file-scope
initialiser — it is what the linker resolves, not something the program computes
— and a table of pointers to globals is the ordinary use. cc1 takes integer
constants there and nothing else. The refusal is honest about what it wants,
which is why this one reads as a gap rather than as a bug.

Seven entries that used to be in this list are gone from it, and each has a case
in `tests/cases` now: a bare `return`, `(*f)(x)`, a function declared in a
block, `#include` by macro, the `const` that belongs to the pointer rather than
the pointee, **brace elision**, and **`<math.h>`**.

**Brace elision was the largest of them.** C90 §6.5.7 makes the braces round a
subaggregate optional: without them you take just enough initialisers to fill
the subaggregate and leave the rest for the next one. cc1 paired item *i* with
element *i* and had no notion of descending, so eight distinct forms failed —
flat, partial, three-dimensional, global, into a struct's array member, into an
array of structs, and into a nested struct. `struct S s = {1, 2}` where `S`
contains an array is ordinary everyday C, not a corner.

What fixed it was replacing that pairing with a **cursor** over the one list.
Filling an object asks three questions in order: a brace stops the descent and
initialises that object whole; a string fills a `char` array; anything else
that is an aggregate descends *on the same cursor* and keeps consuming. That is
the standard's rule stated directly, and it is why `int a[2][2] = {1,2,3,4}`
reaches two elements two items at a time. Both walkers work this way — the one
that emits stores for a local and the one that lays out bytes for a global —
and a third, `skipInit`, moves the cursor without emitting so that
`int a[][3] = {1,2,3,4,5,6}` can infer **two** rows rather than six.

Arrays themselves were never the problem, and the same afternoon established
that: dimensions were tested to eight deep and every one indexes, sizes,
initialises with full braces, decays and passes as a parameter correctly against
gcc. The ceiling is the frame, not a fixed depth. It was only the *elision*.

**`tests/c90-probe.sh` re-derives this whole section.** Each row above is a file
in `tests/c90/`, run against `cc1` and against `gcc -std=c90 -pedantic`, so the
list can be checked rather than believed — which it could not be when it had
two entries in it and eight were missing.

---

## What it accepts and C90 does not

The list above is everything the standard has and this compiler lacks. This is
the opposite list, and until recently there was no way to derive it at all.

`tests/c90/` holds valid C90 that cc1 refuses, so a probe built on it can only
ever find what is *missing*. A feature that is **extra** leaves no trace there —
and none in `tests/run.sh` either, where a case is only written once it passes.
Both suites are silent about the same thing, which is how a compiler claiming
C90 came to accept eight things C90 forbids with two of them written down.
`tests/not-c90-probe.sh` and the corpus in `tests/not-c90/` are that second
question, and they read the same way: an entry flips to "refuses" the day a
check is added.

| | |
| --- | --- |
| `// a comment` | C99 |
| a declaration after a statement in the same block | C99 |
| `for (int i = 0; …)` | C99 |
| `PAIR(, 0)` — an empty macro argument | C99 |
| `long long`, signed and unsigned | C99 |
| `enum E { A, B, };` — a trailing comma | C99 |
| `#define LOG(fmt, ...)` and `__VA_ARGS__` | C99, **kept on purpose** |
| `, ## __VA_ARGS__` | GNU, **kept on purpose** |

The last two were already named as deliberate, and the argument for them stands:
the alternative to a variadic macro in real code is no macro at all. The other
six are accidents of a lexer and a parser being more permissive than the
standard, and none of them costs anything until a program leans on one and then
moves to another compiler — which is exactly when the absence of this list would
have been felt.

`long long` is the one to notice, because it is not only accepted but
*documented*: it sits in the type table under **Types** as though it were part
of the language this compiler implements. It is C99. Sizes and all, it works;
it is simply not C90.

`//` is the sharpest of the six for a different reason. C90 and C99 disagree
about `a //b` silently rather than loudly — a division in one, a comment in the
other — so it is the one extension here that can change what a valid program
means instead of only accepting more of them.

`"ab" "cd"` is the one to be uncomfortable about. Adjacent string literals are
in every C program with a format string too long for one line, and 405 passing
cases never used one — which says something about the corpus rather than about
the compiler. The `int [-1]` in the fourth row is a bug rather than an absence.

**Nine of the fifteen standard headers do not exist**: `assert.h`, `ctype.h`,
`errno.h`, `float.h`, `limits.h`, `locale.h`, `setjmp.h`, `signal.h` and
`time.h`. Shipped are `math.h`, `stdarg.h`, `stddef.h`, `stdio.h`, `stdlib.h`
and `string.h`. Most of the nine are macros and declarations; `float.h` needs
`long double` before it can be honest about `LDBL_*`, and `setjmp.h` needs to
interact with the calling convention, so those two are not mechanical.

**`math.h` was the one worth having first**, because it is the header the
numerical programs this compiler keeps being handed actually reach for — a
Gaussian elimination avoids it, a residual check does not. Like `stdio.h` it is
prototypes only, and the host's libm supplies the code; every C90 function in
it is `double` in and `double` out, since `sqrtf` and `sqrtl` are C99 and there
is no `long double` here to return. The driver names `-lm` on every link, which
glibc requires and macOS ignores, rather than threading "did this program
include `math.h`" out of the preprocessor for no gain. `HUGE_VAL` is `1e400`,
which has no finite double to round to and folds to `+inf` exactly as it does
under gcc — checked before it was written down.

**Three more are declined rather than missing** — old-style function
definitions, trigraphs, and the implicit `int`. All three are required by C90
and all three were removed by a later standard, the first two by C23 and the
third by C99. See [`TYPES.md`](TYPES.md).

The implicit `int` is the newest of the three and the only one that used to be
declined *silently*: `static x = 5;` is a C90 declaration of an `int`, and cc1
answered `expected a type` — a message about where the parser had got to rather
than about the rule, and indistinguishable from a genuine syntax error. It now
says that the declaration has no type, that C90 would read it as `int`, and that
this compiler does not guess. Declining a feature is a decision; leaving the
diagnostic to describe the parser's disappointment is not part of it.

Refused by the backend rather than by the language, because a target reached
what it has not been taught. Each names the target, since the same program
compiles under `-arch x86_64-linux`:

```
codegen: returning a struct or union is not supported yet by the
  x86_64-windows backend
codegen: passing a struct or union by value is not supported yet by the
  x86_64-windows backend
codegen: a struct or union parameter is not supported yet by the
  x86_64-windows backend
codegen: va_start is not supported yet by the arm64-darwin backend
codegen: struct members is not supported yet by the arm64-darwin backend
codegen: aggregate arguments is not supported yet by the arm64-darwin backend
```

The arm64 list is shorter than it was. Floating point and postfix `++` came off
it together, because the program that asked for them wanted both — a `for` loop
with `i++` accumulating into a `float` and a `double` and printing all three
through `printf` is not an unusual program, and it was refused twice over.

`switch` came off it the same way and for the same reason: it was reported from
Xcode as a `switch` over an `int` assigning a `double` in each arm, which is
two register files across one compare-and-branch chain. The backend now emits
the chain the x86-64 one does — the subject once into `x0`, a `cmp` against
each case value, `beq` to its label — and the body as a single statement, which
is what makes fallthrough and Duff's device come out right without being
special-cased. `continue` inside a `switch` searches past it for the enclosing
loop, since a `switch` is a `break` target and not a `continue` target.

Refused because the program is wrong rather than because the compiler is
unfinished:

```
no label 'x' in this function
label 'x' is defined twice in this function
a label cannot be followed by a declaration - put it in a block
the arms of '?:' have incompatible types 'int *' and 'int'
'?:' is not an lvalue, and its address cannot be taken - assign it to
  something first
a call is not an lvalue, and its address cannot be taken - assign it to
  something first
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
'k' is register, and a register object has no address - drop the register
'register' is a storage class for a local or a parameter, and this is file
  scope
'q' is extern, and an extern declaration cannot have an initialiser - the
  definition it names belongs at file scope
'v' is declared 'int' here and 'double' at file scope
argument 1 of 'f' is 'char *' and this is 'int' - only the constant 0 becomes a
  pointer on its own
argument 1 of 'f' is 'int' and this is 'char *' - a pointer is not a number
  here, though a cast makes it one
argument 1 of 'f' is 'char *' and this is 'int *' - a cast says you meant it
the left of '=' is 'char *' and this is 'int' - only the constant 0 becomes a
  pointer on its own
'p' is 'char *' and this is 'int' - only the constant 0 becomes a pointer on
  its own
this function's return type is 'int' and this is 'char *' - a pointer is not a
  number here, though a cast makes it one
a parameter of a definition needs a name - a prototype may leave it out, a
  body cannot
only the first dimension may be left empty - the others decide how far one
  step moves
```

A parenthesis that undoes nothing — `int (f)(void)` — is not a function pointer
and is accepted as the ordinary declaration it is. Telling the two apart is a
question of whether the parentheses wrapped a `*`, which is the whole of the
distinction.

**Reading the system's own headers turns out to be nearly free, and this
document said otherwise.** `<stdio.h>` still resolves to the one this compiler
ships — there is no system include path, and that is deliberate — but the claim
that pointing `-I` at the real headers "would fail on the first `__attribute__`
it met" was a guess, and it is wrong. Six lines get the whole macOS SDK
`<stdio.h>` through the front end:

```c
#define __GNUC__ 4
#define __builtin_va_list long
#define __attribute__(x)
#define __asm(x)
#define __inline
#define __restrict
#include <stdio.h>
```

That compiles, under `-arch x86_64-linux`, to 135 lines of assembly. The chain
is 42 header files and 568 preprocessed lines carrying 43 `__attribute__`, 7
`__asm` and 8 nullability annotations, and defining the extensions away covers
all of them. `-arch arm64-darwin` reaches the end of the same headers and stops
in code generation instead, on a struct member — a gap in that backend, not in
the headers.

Two things keep this an experiment rather than a feature. `__asm("_name")`
renames symbols, so defining it away is a lie the linker eventually presents a
bill for; and `__builtin_va_list` as `long` holds only until something forms a
`va_list` and walks it. "Mostly a project about absorbing GNU extensions" was
right about the shape and badly wrong about the size.

**It also found a real bug**, which is the better argument for having tried. The
SDK defines `_FORTIFY_SOURCE` as `2` followed by a comment and then tests it in
an `#if`; cc1 was keeping the comment in the macro body, so the conditional
evaluator met a `/*` and reported a stray `*`. C removes comments in translation
phase 3, before directives are read in phase 4. Every ordinary use of such a
macro had worked, because the comment travelled into the emitted text and the
lexer dropped it there — only `#if` could ever see it.
`tests/cases/pp_define_comment.c` holds it now.

All three targets emit. `x86_64-linux` is complete; `x86_64-windows` and
`arm64-darwin` are subsets, and each refuses by name what it has not reached.

---

## How it is verified

**Four things run, and they answer different questions.** `tests/run.sh` and
`tests/windows.sh` are ratchets: every case in them was written because it
already passed, so they guard ground already taken and can say nothing about
what is absent. `tests/c90-probe.sh` asks what the standard has and this
compiler lacks. `tests/not-c90-probe.sh` asks the reverse, and exists because
the other three are structurally blind to it. Neither probe can fail a build —
they print and exit 0, because a gap is a fact about today.

The two probes are the explorers and the suites are the ratchet, and the
division matters: 394 cases passed green on a compiler that could not walk a
`const char *`, parse a bare `return`, or declare a function inside a block.
None of the three would ever have surfaced from the suite, because a case is
only written for something that already works.

**Eight cases are directories rather than files**, under `tests/multi/`. Each is
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
| A struct or union in `?:` | 1 |
| Constant expressions | 15 |
| The comma operator and declarator lists | 11 |
| Bit-fields | 13 |
| The preprocessor | 17 |
| Function-like macros | 15 |
| Variadic macros | 8 |
| Unnamed parameters | 6 |
| Separate compilation (directories) | 8 |
| The threaded job loop | 1 |
| The shipped headers, and both spellings of `#include` | 6 |
| File I/O, text and binary | 3 |
| Argument and assignment types | 2 |
| Pointers to functions | 2 |
| Array and struct initialisers | 1 |
| Postfix `++` and `--` | 1 |
| Structs passed and returned by value | 1 |
| Returning a struct through a hidden pointer | 1 |
| Arguments past the registers | 1 |
| Allocation and the byte functions | 1 |
| Escapes and the widest literals | 1 |
| Parenthesised and abstract declarators | 7 |
| `const`, `volatile`, `static` locals | 11 |
| `auto`, and the 32-keyword audit | 1 |
| `register`, and `extern` in a block | 1 |
| A function declared in a block | 1 |
| A bare `return` in a void function | 1 |
| The two-loop float accumulator that arrived as `test.c` | 1 |
| Compound assignment to a subscript, a member and a deref | 2 |
| Brace elision, at block scope and file scope | 1 |
| `<math.h>` against the host's libm | 1 |
| `(*f)(x)`, through a pointer and through an array of them | 1 |
| `#include` by macro | 1 |
| `const` on the pointer against `const` on the pointee | 1 |
| Arithmetic, variables, and the early whole programs | 24 |
| **Apportioned above** | **397** |
| **What `tests/run.sh` runs** | **405** |

**Two totals, because the rows do not account for all of it.** Seven single-file
cases are not in any area above — they were added and the table was not, and the
shortfall was exactly seven when this table last read 387 against a suite of
394, so it has been carried rather than caused. The suite's number is the one to
trust: it counts files, and the areas are a description of them written by hand.
The rows are left summing to what they sum to rather than having a number
adjusted to close the gap, which is how the table came to disagree with the
suite in the first place.

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
nothing about where. The limit moved to the parser to gain that line number,
and then stopped existing at all: an argument past the sixth goes on the stack,
so seven is not a number this compiler has an opinion about any more. What the
episode left behind is the rule rather than the check - a refusal needs a
position to point at - and that outlived the refusal itself.

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
