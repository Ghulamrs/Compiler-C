# How this compiler is checked, and what checking it has taught

`STATUS.md` says what the compiler implements and lists the suites. This
document is the other half: what each suite can and cannot see, and the
specific ways checking has gone wrong here. Every rule below was paid for —
each one is a bug that reached a passing test run, and the commit that fixed
it is named so the claim can be read rather than believed.

Companion documents: [`STATUS.md`](STATUS.md) records what is implemented;
[`BACKENDS.md`](BACKENDS.md) measures the duplication between the two code
generators.

---

## A green suite proves nothing until you know what it ran against

The suites answer *is this binary right*. They never answer *is this binary
the one I changed*, and the difference is invisible: a copy that did not land,
or a `git pull` that aborted, produces a **passing** run that means nothing
and reads exactly like success.

This has happened twice. Both times a relay to another machine failed —
once a `tar` that never extracted, once a `pull` that aborted over untracked
files *after* printing its "Updating ..." line — and both times `make` found
nothing to rebuild, the suite passed, and the result was reported as
verification of code that had never been compiled.

`tests/fingerprint.sh` cannot catch this either. It compares output against a
record, and a stale binary reproduces the stale record perfectly.

So check provenance separately, and cheaply:

```sh
sha256sum src/backend/X86_64Linux.cpp          # against git show HEAD:<path>
[ cc1 -nt src/Parser.cpp ] && echo "built from these sources"
grep -c xorpd out.s                            # a token the change introduces
```

The third is the fastest and the most convincing: ask the *output* whether the
fix is in it. That is what unmasked the second occurrence.

## A count without names is not a result

`msvc/run-corpus.ps1` reported `cc1 refused: 2` and `link fails: 1`, and named
one of those three cases anywhere. A bug sat under each unnamed number for a
day while the totals looked settled — `pp_include`, where a Windows-hosted
compiler could not resolve `#include "beside.h"`, and `lib_math`, where the
`$` mangling escaped onto an imported symbol and `link.exe` answered
`unresolved external symbol $fabs`.

A case that fails for a good reason and a case that fails for a bad one add up
to the same integer. Every row of the corpus table in `STATUS.md` carries its
case names now, and `run-corpus.ps1` prints the name of anything it excludes
along with the reason.

## The fingerprint certifies stability, not correctness

`tests/fingerprint.sh` records a sha256 for all cases against each of the four
spellings and names every case that moved. It is the right tool for *did this
refactor move any byte* — it proved the emission seam and the shared `Walker`
changed nothing, across hundreds of files, which no differential suite could
have shown.

It is the wrong tool for *are the bytes right*. For a day every one of its
digests notarised a floating negation that lost the sign of zero. A refactor
should re-record it only after the differential suites pass, and a changed
digest is a question, not a verdict.

Two properties make it a repository artefact rather than one machine's habit:
it needs no assembler, so it covers targets the host cannot execute; and the
same file verifies on every host. That second one was not free — `<assert.h>`
expands `__FILE__` into the assembly, so the harness must name its sources
relative to the repository root or the fingerprint records where the
repository happens to sit.

## `-S` succeeding is not evidence the backend is right

Counting how many cases a target "compiles" is a table in
`help/command-lines.md`, and it cannot see an instruction the assembler will
reject. arm64's `double`→`long double` emitted `fcvt d0, d0` — no same-width
form exists — and scored a clean pass in that column. Only assembling and
running found it.

Where a target is native, compile it *and* run it against the platform's own
compiler. Where it is not, at least assemble it.

## Two lowerings answer the same C, so divergence is the house bug class

`X86_64Linux.cpp` and `Arm64Darwin.cpp` are independent walks of one AST, and
the bugs that survive longest are the ones where they disagree and only one is
wrong. Three were live at once until `ca4bff0` and `646d11b`:

| | x86-64 | arm64 |
| --- | --- | --- |
| `unsigned long long` → floating | converted as **signed**: `~0ULL` printed `-1.0` | correct (`ucvtf`) |
| unary minus on floating | `0.0 - x`, losing the sign of zero | correct (`fneg`) |
| canonicalising a narrow return | every shape | **one of six** |

**When one backend is wrong, read the other one first.** The answer is usually
already there, and a divergence is far likelier than both being wrong the same
way. None of the three had a test, and each was one `printf` differential away
from being caught — `printf("%f", -0.0)` prints the sign.

The shared `Walker` exists partly for this reason: a statement's control flow
is now one text, so that class of drift cannot recur below the expression
layer.

## The suite is a ratchet; the probes are the explorers

Every case in `tests/cases` was written because it already passed, so a green
run says nothing about what is absent. `tests/c90-probe.sh` asks what the
standard has and this compiler lacks; `tests/not-c90-probe.sh` asks the
reverse, and exists because the other two are structurally blind to it — a
compiler claiming C90 had accumulated eight extensions with two written down.

Neither probe can fail a build. They print and exit 0, because a gap is a fact
about today rather than a regression.

## Measure on every host, not on the fastest one

A change measured as free on one machine cost 28% under `gcc` on another. Two
hosts disagree about more than speed: an x87 host folds `0.1 + 0.2` at 80 bits
where the expression's type is `double`, which is how `54a8cd7` found that the
constant folder was working in the host's widest type rather than the
expression's own.

The same axis catches host-versus-target confusion. When something is wrong
only on Windows, ask whether it is the **target's** widths (`long` is 4 bytes
there) or the **host's** filesystem (a compiler *hosted* on Windows sees
backslashes) before assuming the backend. Both classes have appeared:
`361e81e` was the first, `24506b1` the second.

## A measurement is code, and rots like code

`tools/backend-overlap` classifies a line as emission by matching `out_ <<`.
When x86-64 moved to the `Spelling` seam, its 364 `a_->ins(...)` calls stopped
matching, and the tool reported **one** emit line in a file that emits
constantly — inflating the duplication figure that `BACKENDS.md` rests its
argument on, in the direction that argues *for* an intermediate
representation.

A tool that measures the code must be re-derived when the code it measures
changes shape, and a number that drifts toward the conclusion you were already
considering deserves the most suspicion, not the least.

## Corpus shape is not corpus size

`tests/challenge.sh --heavy` holds 432,000 lines in twelve files. That is the
right volume and the wrong shape for judging a thread pool: twelve units
cannot occupy twelve threads, and a run is as long as its slowest unit. The
same lines in 240 units — `--units`, written by `tools/gen-corpus` — reverse
the sign of the answer, from −2.9% to +10.6% on a one-core box and from
nothing to +220% on a twelve-core one.

The `-j` column was measuring how badly twelve divides. When a benchmark
answers a question you did not ask, suspect the corpus before the code.

## Prove a new case can fail

A case added alongside a fix should be run against the *unfixed* compiler
once, and seen to fail. Two of the cases added in `ca4bff0` were checked this
way — the fix reverted, the case run, the wrong output read — and it is the
only thing that distinguishes a test from a comment.

The same discipline applies to the suite itself: each increment ends with a
deliberate injection, because a suite that has never failed is unproven.
