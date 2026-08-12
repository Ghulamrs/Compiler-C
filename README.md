# ansicc

An ANSI C compiler, written by hand, targeting x86-64 System V.

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
make            # serial, deliberately - see the note in the Makefile
make test
```

## The shape

Three stages, one direction, no passes over the same data twice:

| File | Does |
| --- | --- |
| `src/lex.c` | source text → token list |
| `src/parse.c` | tokens → tree, recursive descent |
| `src/codegen.c` | tree → x86-64 assembly |
| `src/main.c` | the driver; emits `.s` only |

Assembling and linking are left to `gcc`. That keeps the surface under test to
the part actually being written, and it is what makes the differential suite
below possible.

Written in C rather than C++: a C++ translation unit of this shape costs about
166 MB to compile on this box, which has around 200 MB spare. C costs a
fraction of that. It also leaves self-hosting open — feeding the compiler to
itself is the only test that exercises every corner at once.

## Testing

`tests/run.sh` compiles every case **twice**, once with `cc1` and once with
`gcc`, runs both, and requires that the two agree *and* that both match the
exit code written at the top of the case.

Comparing against gcc rather than against expectations alone is the point: an
expectation is an opinion about C, while gcc is the reference implementation
sitting on the same disk. Where they disagree, the case is wrong until the
standard says otherwise.

## Accepted today

```
program = "int" "main" "(" ["void"] ")" "{" "return" expr ";" "}"
expr    = mul ("+" mul | "-" mul)*
mul     = unary ("*" unary | "/" unary)*
unary   = ("+" | "-") unary | primary
primary = num | "(" expr ")"
```

Integer constants, the four arithmetic operators with correct precedence and
left associativity, unary plus and minus, parentheses, and both comment forms.
Everything else is still to come.

## Design

[`docs/TYPES.md`](docs/TYPES.md) settles the type system before any of it is
built: the model, what the Target owns rather than the front end, the
conversion rules, what code generation has to do differently once a value is
not always eight bytes, and the four stages it lands in.

## Not done yet

Variables, statements, control flow, functions, types, pointers, arrays,
structs, the preprocessor. In roughly that order.
