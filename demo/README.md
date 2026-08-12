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

## What the suite does *not* cover

One thing it is important not to read into a green run. The code generator pads
%rsp to a 16-byte boundary before every call, because System V requires it.
Removing that padding entirely still leaves **56 of 56 passing** - so nothing
here proves it works, and nothing would notice if it broke.

The reason is that misalignment only bites when the callee executes an
instruction that demands it, typically an aligned SSE store inside printf when
a floating-point argument is involved. That needs string literals and doubles,
neither of which exists yet. The padding stays because the ABI says so, not
because a test says so, and this note is here so the next person does not
assume otherwise.
