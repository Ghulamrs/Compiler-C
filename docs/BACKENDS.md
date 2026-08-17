# Two code generators, and how much of them is one

Whether this compiler should grow an intermediate representation is the largest
open question about its shape, and it has so far been argued from impressions.
This document replaces the impressions with a measurement, so that the decision
can be made against a number and revisited when the number moves.

Everything below is re-derived by `tools/backend-overlap`, which reads source
and needs nothing built. Do not quote these figures without running it — this
document is a snapshot of 17 August 2026, re-derived after the shared walker, and the whole point of the tool is
that the snapshot expires.

Companion documents: [`STATUS.md`](STATUS.md) records what the compiler
implements; [`PARALLELISM.md`](PARALLELISM.md) the other structural question
settled early; [`VERIFYING.md`](VERIFYING.md) has a section on why a
measurement like this one rots, and how this document's figures went wrong.

---

## The question

`src/backend/` holds three platforms but only **two lowerings**.

`X86_64Linux.cpp` serves System V *and* Microsoft x64 from one instruction
selection, because the calling convention is data: `X86_64Windows.cpp` is 75
lines of tables and hands back the same generator, and `Masm.cpp` respells that
generator's output for `ml64` rather than selecting instructions of its own.
That half of the design works, and the file sizes are the evidence.

`Arm64Darwin.cpp` is the other thing entirely — a second, independent walk of
the same AST, emitting AAPCS64. Nothing is shared with the first beyond the tree
they both read.

So: how much of that second lowering is genuinely a different answer to a
different machine, and how much is the first one typed again?

## What was counted

Every line of every function body is called one of two things.

| | |
| --- | --- |
| **emit** | it writes assembly text — `out_ << "..."`, or a call through the `Spelling` seam, `a_->ins(...)` |
| **logic** | everything else: traversal, control flow, questions about types, offsets, labels |

The division is the one that matters for this question, because an IR **absorbs
logic and keeps emit**. Logic appearing in both files is duplication an IR
removes. Emit is work it does not touch, and would not reduce by a line.

Blank lines, comments and lines holding only a brace are counted as neither,
which is why the totals below are smaller than `wc -l` over the same files.

## The totals

| | emit | logic | counted | file |
| --- | --- | --- | --- | --- |
| `X86_64Linux.cpp` | 400 | 637 | 1,037 | 1,485 |
| `Arm64Darwin.cpp` | 190 | 527 | 717 | 1,080 |

**43 functions exist in both files. 428 logic lines are duplicated** — counting,
for each function present in both, the smaller of the two logic counts.

That is the headline, and on its own it understates the case.

## The sharper report

For each visitor present in both files, strip the emission lines and the
differences that are spelling rather than substance — the class name, `if (p)`
against `if (p != nullptr)`, a brace initialiser written with its type name —
and ask whether what is left is the same text.

**Three of thirteen are identical as written:** `Cast`, `MemberAccess`,
`StrLit`.

That was thirteen of twenty-seven before `Walker` took the statement visitors
into one text. What is left to compare is the expression layer, which is the
part that was never the easy half.

The `For`/`DoWhile` pair that differed by one line each — `label("step", id)`
against `label("cont", id)` — is gone with the statement visitors, and so is
the drift itself: the walk names its labels once now, and a target that wants
a different name has to say so as a primitive.

## Where the difference is real

The remaining ten are not evenly difficult. Ranked by how much of the
algorithm actually changes:

(*Lines compared* is what the visitor report weighs: every non-emission line,
signature and braces included. It is a larger figure than the *logic* column of
the first table, which drops those — `Call` is 155 lines compared and 138 lines
of logic.)

| Visitor | Lines compared | Lines that differ |
| --- | --- | --- |
| `Call` | 155 | 152 |
| `Binary` | 44 | 53 |
| `Return` | 31 | 30 |
| `Postfix` | 31 | 27 |
| `Unary` | 25 | 26 |
| `Assign` | 22 | 18 |
| `VaArg` | 17 | 15 |
| `Num` | 15 | 14 |
| `VaStart` | 6 | 4 |
| `Var` | 4 | 4 |

`Call` alone is 138 of the 637 logic lines on the x86-64 side — a fifth of that
file's logic — and it is the one place
where almost nothing is shared. That is not an accident of how it was written:
argument classification, register assignment, stack alignment and the return
path are exactly what an ABI *is*, and System V, Microsoft x64 and AAPCS64
genuinely disagree about all four.

It is also where arm64's last refusals lived — `va_start`, calls through a
function pointer, arguments past the eighth register, and aggregates that do not
fit the registers left for them. All of them are implemented now and arm64
refuses nothing, but they were the last to land, and that they were all in
`Call` is the same point from the other side.

## What an IR would and would not buy

**Would:** remove on the order of **428 duplicated logic lines**, and remove the
class of bug that duplication produces. Three of the four bugs the clang
differential has ever found on arm64 were the second lowering failing to
re-derive something the first had already solved — integer results not narrowed
to their own width, a register clobbered in the prologue, a 12-byte struct
stored a register too wide. Each was fixed once on x86-64 and then again,
later, on arm64.

**Would not:** touch `Call`, or the ~640 lines of genuine instruction selection.
The ABI differences are differences in the target, not artefacts of the
structure, and they survive any amount of shared plumbing.

So the trade reads plainly, and it is not flattering in one direction: an IR
removes the duplication that has been **cheap** — statement visitors that were
easy to write twice and have never gone wrong — and leaves untouched the part
that has been **expensive**. The strongest argument for it is not the 428 lines.
It is that a third target would otherwise be a third lowering — and a third
lowering is cheaper than it was, because `Walker` already holds the statement
walk and a new target inherits it and writes its own `Call`.

The growth argument this document used to make has been answered rather than
confirmed: `Arm64Darwin.cpp` went 781 → 1,378 lines and the duplication with
it, and the walker took it back to 1,080 with 428 duplicated logic lines. The
trend was real; extracting the shared walk was the cheaper response to it.

The strongest argument against is that `STATUS.md` opens by claiming four
stages, one direction, and no passes over the same data twice. An IR ends that,
and the directness is not nothing — it is most of why the compiler can be read.

This document does not settle it. It makes the two sides quote the same numbers.

## What this does not measure

- **Nothing about correctness.** Two identical visitors could be identically
  wrong; the differential suites answer that question and this tool does not.
- **Nothing about `Masm.cpp`**, which is a translation of emitted text and has
  no lowering in it to share.
- **Emit lines are counted, not weighed.** One `out_ <<` writing a whole
  prologue counts the same as one writing `ret`.
- **The two generators do not emit the same way**, and the tool had to be
  taught the difference. x86-64 goes through the `Spelling` seam and arm64
  writes text directly; a pattern matching only `out_ <<` reported *one* emit
  line in a file that emits constantly, and counted 364 `a_->ins(...)` calls
  as logic. That inflated the duplication this tool exists to measure - 469
  against a true 428 - and it inflated it in the direction that argues for an
  IR, which is the direction a measurement should be least trusted to drift.
- **The alias table is hand-made.** Five pairs do the same job under different
  names — `emitFunction`/`emit`, `narrowInt`/`canonicalise`, `pushD`/`pushF`,
  `popD`/`popF`, `storeThrough`/`store`. Each was read before being paired.
  A sixth such pair, added later and not noticed, would be counted as two
  separate functions and would make the duplication look smaller than it is.

## How this was first measured wrongly

Worth recording, because the trap is easy and the first answer was published
before it was caught.

The first pass pulled each function out with an `awk` range — from the
signature to the next line beginning with `}`. That is correct for a function
whose body spans lines and **silently wrong for a one-liner**, where the closing
brace is on the signature's own line. `visit(const ExprStmt &n) { n.expr().accept(*this); }`
and `visit(const StrLit &n) { genAddr(n); }` are both one-liners in both files,
so each swallowed the function that followed it. `ExprStmt` was reported as a
30-line visitor that differed, when it is a 1-line visitor that is identical,
and `StrLit` was double-counted with `MemberAccess`.

The count that came out was 12 of 26 identical. The right count is 13, and the
duplicated-logic figure was 424 rather than 436. Both errors happened to point
the same way — they made the two files look *more* different than they are.

`tools/backend-overlap` matches braces instead, ignoring braces inside string
literals, which is why it is a program in the repository rather than a command
someone once ran.

## Re-deriving it

```
tools/backend-overlap            the full tables and the visitor report
tools/backend-overlap --terse    the totals alone
```
