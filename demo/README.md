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
  jmp .L.return
  mov $0, %rax                 # falling off the end of main returns 0;
                               # the return above jumps over this
.L.return:
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

That is the **only channel a compiled program has**. There are no function
calls yet, so no `printf`, no file, no console. The single value a program can
communicate is whatever `main` returns.

Two consequences:

- **The range is 0 to 255.** The kernel keeps only the low byte, so
  `return 300` arrives as 44 and `return -1` as 255. Every case in the suite is
  written to land inside that window, which is why the test is `factorial(5)`
  and not `factorial(10)`.
- **One number per program.** No intermediate values and no trace. A wrong
  answer says that something is wrong, never where.

The functions increment lifts this: once calls and the argument registers work,
`printf` becomes callable and a program can say more than one byte on its way
out.

## How this is checked

The suite does not trust the number above. It compiles every case twice — once
with `cc1`, once with `gcc` — runs both under a five second limit, and requires
that the two agree *and* that both match the `// expect:` line at the top of
the case.

```
PASS: 40   FAIL: 0
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
