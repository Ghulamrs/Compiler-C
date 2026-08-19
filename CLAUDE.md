# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## The language this is written in

`src/` is **ISO C++14**, and anything added to it must be too. All three
toolchains pin it: `-std=c++14 -Wall -Wextra -Werror -pedantic` in the
`Makefile`, `<LanguageStandard>stdcpp14</LanguageStandard>` in
`msvc/cc1.vcxproj`.

**A Mac cannot enforce this, and that is the trap.** Apple's libc++ hands you
`std::string_view` under `-std=c++14`, so a C++17-ism compiles clean there,
passes the suites, and is refused only when it reaches real g++. The same file
that builds with `clang++ -std=c++14` on a Mac fails under `g++ -std=c++14`
with `'string_view' is not a member of 'std'`. **A change that compiles on a
Mac has been checked against Apple's library, not against the standard.**

The one feature that ever wanted C++17 was `string_view`, for the borrowed
text an operand carries. It is written by hand instead — the four-operation
`Str` in `src/backend/Spelling.h`. Its comment says it is not a string class
and nothing should grow it into one; that is a decision, not an omission.

## Build and test

```
./build              build (wraps make in a memory cgroup where one is available)
./build test         build and run the full Linux suite
./build clean
```

Use `./build` rather than bare `make`: a class-heavy translation unit here was
measured at 142 MB, and the build is serial by design for that reason.

`make test` runs six suites and **not** the whole set — `run.sh`, `windows.sh`,
`driver-modes.sh`, `debug.sh` (twice: host target and `x86_64-windows`) and
`fingerprint.sh`. Do not read "all suites green" into it.

Individual suites, and what each needs:

| Suite | Needs | What it asks |
| --- | --- | --- |
| `tests/run.sh` | Linux + gcc | the whole corpus, differential against gcc |
| `tests/windows.sh` | Linux | Microsoft-ABI code, run natively (see its header) |
| `tests/debug.sh [arch]` | gdb or lldb | what `-g` produced, asked of a debugger |
| `tests/arm64.sh` | an arm64 Mac | the arm64 backend against clang |
| `tests/cross-abi.sh` | any host with a C compiler | cc1's objects linked against the host compiler's |
| `tests/fingerprint.sh` | nothing | every byte of every target's assembly against recorded digests |
| `tests/masm-native.sh`, `tests/windows-native.sh` | a Windows host over ssh | ml64/link and clang/PE, natively |
| `tests/c90-probe.sh`, `tests/not-c90-probe.sh` | a C compiler | what the language says vs what this accepts |

One case, or a serial run for debugging:

```
./tests/run.sh gcd          cases whose name contains "gcd"
./tests/run.sh '' 1         serially
./tests/debug.sh x86_64-windows    the debug corpus against the Microsoft ABI
```

## Architecture

A single pass per stage, no IR:

`Driver` → `Preprocessor` (text in, text out, with `#line` records) → `Lexer`
→ `Parser` (builds `Ast.h` and does all type work in `Type.cpp`) → a backend.

**`Source` owns the preprocessed text** and turns a byte offset into a file,
line and column. Diagnostics and the line table both go through it, so the two
can never disagree about where something was written.

**The backends share one statement walk and nothing else.** `backend/Walker`
holds fourteen visitors, the jump stack and the label discipline in one text;
each target supplies five one-line primitives (compare-and-branch, jump,
label, case-compare). Expressions stay per-target — `Call` most of all, since
that is where the ABIs genuinely part. A difference between targets belongs in
a primitive, where it is visibly a decision.

**Three things vary per target and they are separate axes.** `Target` answers
what the types measure; `Abi` answers how arguments travel; `Spelling` answers
how an instruction is written down. One instruction stream serves both x86-64
targets — `GnuSpelling` and `MasmSpelling` are the whole of what differs
between them, which is why `-masm=gnu` and MASM produce the same program.

**`backend/Dwarf.cpp`** writes the debug information for the targets that can
carry it. `-g` is refused for `x86_64-windows` in the MASM spelling — ml64
builds no line table, and a native Windows debugger wants CodeView rather than
DWARF. The comment at the top of `backend/X86_64Windows.cpp` records what was
measured about that, so it need not be measured again.

## The bug class this project produces

**Cross-target divergence** — two lowerings answering the same C differently.
When one backend is wrong, ask what the other one does before assuming both
are: the answer is usually there, and a divergence is far likelier than both
being wrong.

**Being hosted on a platform is a separate axis from targeting it.** Bugs have
hidden in `directoryOf` treating `/` as the only separator, and in symbol
mangling escaping onto imported names — neither reachable from the machines
this is usually developed on.

## Verification

**Prove the artefact, not the exit status.** A green suite proves nothing
until you know what it ran against. `make` with nothing to rebuild, a relay
that silently failed, or a binary older than its sources will all report
success.

- Check the binary is newer than its sources: `[ cc1 -nt src/Parser.cpp ]`
- Grep the emitted assembly for a token the change introduces — the cheapest
  confirmation there is
- `tests/fingerprint.sh` is 1,672 digests, which is every case for every
  target variant; if a change is meant to alter nothing, this is what says so

When a suite fails on one host and passes on another, **suspect the host's
tools before the compiler**. `timeout` is GNU coreutils and absent on macOS,
which once presented as eight ABI failures in `cross-abi.sh`.

Development happens on more than one machine (see the README). When relaying
files rather than pulling them, hash them on the far side against
`git show HEAD:<path>` — a relayed tree is not a checked-out one, and `git
pull` aborts over untracked files it would overwrite *after* printing
"Updating", so the failure reads like success.
