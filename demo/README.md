# A worked example

One program taken all the way through, kept in the repository so the output of
the compiler can be read without having to run it first.

The program is [`tests/cases/gcd.c`](../tests/cases/gcd.c) — the same file the
differential suite uses, not a copy that can drift from it. The assembly beside
this file, [`gcd.s`](gcd.s), is what `cc1` actually emitted.

Regenerate both with `./demo/refresh.sh`. If `gcd.s` no longer matches what the
compiler produces, that script will say so — a recorded artifact that silently
goes stale is worse than none.

## The program

```c
// expect: 6
/* Euclid, which is why % had to exist */
int main(void)
{
    int a = 48;
    int b = 18;
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}
```

## The commands

```
$ ./cc1 tests/cases/gcd.c -o gcd.s     # our compiler: C to assembly
$ gcc gcd.s -o gcd                     # gcc as assembler and linker only
$ ./gcd; echo $?                       # run it, and read the exit status
6
```

`gcc` never sees the C. It assembles text we produced and links it. What it
contributes is the assembler, the linker and the C runtime startup — not any
part of the translation.

## The assembly, annotated

The machine-generated original is `gcd.s`; the comments below are added here
for reading.

```gas
  .globl main
  .text
main:
  push %rbp                    # establish the frame
  mov %rsp, %rbp
  sub $32, %rsp                # three locals, 24 bytes, rounded to 32:
                               # the ABI wants %rsp 16-byte aligned
  mov $48, %rax                # int a = 48
  mov %rax, -8(%rbp)
  mov $18, %rax                # int b = 18
  mov %rax, -16(%rbp)
.L.begin.0:                    # while (b != 0) {
  mov $0, %rax                 #   the right operand is generated first
  push %rax
  mov -16(%rbp), %rax          #   load b
  pop %rdi
  cmp %rdi, %rax               #   compares b - 0
  setne %al                    #   one byte: 1 if not equal
  movzb %al, %rax              #   widened, or a stale high byte joins in
  cmp $0, %rax
  je .L.end.0                  #   condition false, leave the loop
  mov -16(%rbp), %rax          #   int t = a % b
  push %rax
  mov -8(%rbp), %rax
  pop %rdi
  cqo                          #   sign-extend into %rdx:%rax for idiv
  idiv %rdi
  mov %rdx, %rax               #   the remainder, not the quotient
  mov %rax, -24(%rbp)
  mov -16(%rbp), %rax          #   a = b
  mov %rax, -8(%rbp)
  mov -24(%rbp), %rax          #   b = t
  mov %rax, -16(%rbp)
  jmp .L.begin.0               # }
.L.end.0:
  mov -8(%rbp), %rax           # return a
  jmp .L.return.main
  mov $0, %rax                 # falling off the end of main returns 0;
                               # the return above jumps over this
.L.return.main:
  mov %rbp, %rsp
  pop %rbp
  ret
```

Three slots at `-8`, `-16` and `-24` hold `a`, `b` and `t`. The code is poor on
purpose: every intermediate goes through the stack because register allocation
is a later, separable problem, and a stack machine is always correct.

## The result, and what a result can be

```
exit status = 6
```

The exit status is still how an *answer* comes back, and it is still one byte:
the kernel keeps only the low eight bits, so `return 300` arrives as 44 and
`return -1` as 255. Cases are written to land inside that window - which is why
the test is `factorial(5)` rather than `factorial(10)`, and why
`fn_arg_is_call` computes `sum(fact(3))` instead of `fact(fact(3))`. The latter
is 720, and it came back as 208 the first time it was written.

Since calls arrived, a program is no longer *limited* to that. `putchar` needs
only its prototype, and then a program can print:

```c
int putchar(int c);
int stars(int n)
{
    int i = 0;
    while (i < n) { putchar(42); i = i + 1; }
    putchar(10);
    return 0;
}
int main(void)
{
    int r = 1;
    while (r <= 5) { stars(r); r = r + 1; }
    return 0;
}
```

```
*
**
***
****
*****
```

The suite compares both channels now - what a program prints and what it
returns - because a compiler can get the answer right and the output wrong.

## How this is checked

The suite does not trust the number above. It compiles every case twice — once
with `cc1`, once with `gcc` — runs both under a five second limit, and requires
that the two agree *and* that both match the `// expect:` line at the top of
the case.

```
PASS: 56   FAIL: 0
```

When something breaks it names it. Removing the `mov %rdx, %rax` above, so that
`%` yields the quotient, gives:

```
FAIL collatz - our binary did not terminate within 5s
FAIL gcd - cc1 gave 9, gcc gave 6
FAIL mod - cc1 gave 2, gcc gave 3
FAIL mod_prec - cc1 gave 7, gcc gave 8

PASS: 36   FAIL: 4
```

Not spilling the parameter registers into their frame slots gives 11 failures.

## The alignment padding, now proved

This section used to say that the 16-byte stack alignment before a call was
unverified — that deleting it left every case passing, because nothing the
compiler could call cared. That is no longer true.

`printf` with a floating argument makes libc save its register area with an
aligned SSE store, and a misaligned `%rsp` faults on it. With the padding
removed, `fp_printf_deep` exits **139**, which is SIGSEGV, against gcc's 0.

The padding is only emitted where an odd number of values is already on the
stack at the call, so most calls need none. That is why the case had to put
the call inside a larger expression to catch it.

---

# A second worked example: functions

`gcd` above is one function and no calls. This one is the other half — two
prototypes, a call to a function defined further down, recursion, a computed
argument, and output. The case is
[`tests/cases/out_factorial.c`](../tests/cases/out_factorial.c); the assembly is
[`out_factorial.s`](out_factorial.s).

## The program

```c
// expect: 0
/* Prototypes first: fact is called above the line that defines it, and
   putchar is not defined here at all. Both are checked against these. */
int putchar(int c);
int fact(int n);

int main(void)
{
    int n = 5;
    int f = fact(n);

    putchar(48 + f / 100);       /* 120, one digit at a time */
    putchar(48 + f / 10 % 10);
    putchar(48 + f % 10);
    putchar(10);
    return 0;
}

int fact(int n)
{
    if (n <= 1) { return 1; }
    return n * fact(n - 1);      /* recursion, and a computed argument */
}
```

```
$ ./out_factorial
120
$ echo $?
0
```

113 instructions. Four things in them are worth reading closely.

## 1. How a call is made

```gas
  mov -8(%rbp), %rax     # evaluate the argument
  push %rax              #   onto the stack
  pop %rdi               #   then into the first argument register
  mov $0, %rax           # %al = 0: no vector registers used, which a
  call fact              #   variadic callee reads. printf will need this.
  mov %rax, -16(%rbp)    # the return value arrives in %rax
```

Arguments go via the stack rather than straight into registers. With one
argument that looks absurd — `push` then immediately `pop` — and it is: there
are **six such round trips in these 113 instructions**. But evaluating directly
into `%rdi` does not survive two arguments, because computing the second can
call something that overwrites the first. Correct first, then fast.

## 2. How recursion works, with no special case for it

```gas
fact:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  mov %rdi, -8(%rbp)     # the parameter is spilled to its own slot
  ...
  call fact              # nothing here knows this is recursion
  push %rax
  mov -8(%rbp), %rax     # n, reloaded - the recursive call did not disturb it
  pop %rdi
  imul %rdi, %rax        # n * fact(n - 1)
```

`n` survives the recursive call because it lives in this frame, not a register.
That is the stack machine paying for itself: a register allocator has to prove
what survives a call, and this does not have to.

## 3. Where the stack alignment appears — and where it does not

System V requires `%rsp` to be 16-byte aligned at a `call`. Every push moves it
by 8, so the padding is needed exactly when an odd number of values is
outstanding:

| case | calls | `sub $8, %rsp` emitted |
| --- | --- | --- |
| `out_factorial` | 6 | 0 |
| `fn_align` (`1 + f(2) * 2`) | 1 | 1 |
| `fn_nested` | 3 | 1 |

In `fn_align` the `2` of `* 2` is already on the stack when `f(2)` is called,
so the depth is odd and the padding appears:

```gas
  mov $2, %rax
  push %rax
  pop %rdi
  sub $8, %rsp     # <- odd depth, realign
  mov $0, %rax
  call f
  add $8, %rsp
```

**But note what is and is not verified.** That the padding is *emitted* where
expected is checked above. That it is *necessary* is not: deleting it entirely
leaves all 56 cases passing, because `putchar` never executes an instruction
that requires the alignment. The proof needs `printf` with a floating-point
argument, and that needs string literals and doubles.

## 4. The epilogue, and the return that jumps over it

```gas
  mov $1, %rax
  jmp .L.return.fact     # return 1
...
  mov $0, %rax           # falling off the end returns 0
.L.return.fact:
  mov %rbp, %rsp
  pop %rbp
  ret
```

Every function has one exit. A `return` anywhere jumps to it, and the
fall-through case sets 0 just above the label so the jump skips it — which is
what C promises for `main` and costs one instruction everywhere else.

The label carries the function name, `.L.return.fact`, because there is more
than one function now. That change is what made the recorded `gcd.s` go stale,
and `refresh.sh --check` is what noticed.
