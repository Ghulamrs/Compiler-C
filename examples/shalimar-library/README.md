# A library for Shalimar to call

`stats.c` is ordinary C89. What makes it a *Shalimar* library is one thing: it
takes `ShmArray *`, the opaque handle a Shalimar array arrives as.

```
./build.sh          # cc1 compiles it, ar makes libstats.a
CC=cc ./build.sh    # or the host's compiler
```

Then, from `Compiler-S/examples/using-a-library`:

```
shc prog.shm --with=../../../Compiler-C/examples/shalimar-library/libstats.a -o prog
```

## Three things worth knowing before you write your own

**A compiler does not make a library.** cc1, cl and gcc make *objects*; `ar`
(or `lib.exe` on Windows) makes a library out of objects. Two steps.

**No `main`.** The Shalimar runtime owns it. A `main` here collides at the
link, and the message names a duplicate symbol rather than this file.

**A rank-2 array is nested.** `shm_get_real` on a `real[][]` reads a row
*reference* as a double and returns nonsense **without any diagnostic** — the
worst kind of wrong. Take the row with `shm_get_ref` first; `stats_grand_total`
shows the shape.

## What the Shalimar side has to say

Nothing is inferred. Each function must be declared, in Shalimar's own types:

```
uses <real> = stats_mean(a[]: real)
```

The declaration is the whole contract — this is all `shc` will ever know about
the function, and calls are checked against it exactly as against a function
you wrote. `Compiler-S/docs/FOREIGN.md` is the reasoning;
`Compiler-S/docs/ARRAY-ABI.md` is the handle.

## The limit

Only signatures Shalimar's types can express: `int`, `real`, `char`, and arrays
of them. There are no pointers in Shalimar, so a function taking `char *` or
returning one cannot be declared however you write it. That is why this library
takes handles and doubles and nothing else.
