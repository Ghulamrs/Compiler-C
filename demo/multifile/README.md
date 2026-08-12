# Two translation units

C's answer to "more than one source file" is separate compilation: each file is
compiled alone, knowing nothing of the others, and the linker joins them. This
compiler has been able to do that since globals arrived; the driver now also
accepts several files in one invocation.

```
$ cc1 stack.c main.c        # one .s per input
$ gcc stack.s main.s -o prog
$ ./prog
depth after pushes: 5
popped: 25 16 9 4 1
depth after pops: 0
```

`main.c` has never seen `stack.c`. It has only the three declarations at its
top, which is exactly what a header would have given it.

`stack.c` keeps its storage private with `static`, and that is real rather than
decorative — the linker sees the difference:

```
$ nm prog | grep -E 'the_stack|depth'
0000000000404050 D depth        <- external, main.c reads it
000000000040400c d the_stack    <- internal, invisible outside stack.c
```

## On doing this in parallel

The jobs are independent by construction: each builds its own `Source`,
`TypeTable`, `Parser` and `CodeGen`, and shares nothing. So the loop over jobs
could become a loop over threads without touching the compiler.

It has not, because measurement says it would not pay. Over all 191 test
programs on this box:

```
serial       0.304 s
2 processes  0.221 s
4 processes  0.219 s   (there are only 2 vCPUs)
```

Eighty milliseconds, most of it process startup. For scale, gcc takes 2.59 s
over the same files — this compiler is faster only because it does far less.

There is also a reason not to look for parallelism *inside* one file. C cannot
be parsed without the symbol table built by everything before: `(A)*b` is a
cast if `A` was typedefed and a multiplication otherwise, and the typedef may
be five hundred lines earlier. Splitting a token stream into chunks to parse
concurrently would first have to parse the declarations to know where the
chunks may safely fall - which is the sequential work it set out to avoid.
Parallelism in C compilation lives at the file, and `make -j` already knows how
to schedule it.
