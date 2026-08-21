// expect: 42
/* A by-reference aggregate that arrives on the *stack*, which is a shape this
   corpus did not have.

   Microsoft x64 returns a struct wider than eight bytes through a hidden first
   pointer, so a function with four declared parameters has five argument
   slots: the return, then three in %rcx, %rdx, %r8/%r9, and the fourth on the
   stack. A struct parameter travelling by reference therefore arrives as a
   pointer read out of a stack slot - and the prologue used to load that
   pointer into the same register msCopyToSlot moves the bytes with, so it
   survived exactly one eight-byte move and every field after the first was
   read from wherever those bytes happened to point.

   ag_stack_and_union.c has eight arguments and a struct, and does not catch
   this: 'long' is four bytes on this target, so its struct is eight and takes
   the in-register road. The struct here is twenty-four.

   Found by building a 2,300-line program with cc1 for x86_64-windows: it
   linked, then faulted on its first arithmetic. */
struct V {
    int kind;
    union {
        double number;
        struct { const char *text; int length; } string;
    } as;
};

static struct V number(double d)
{
    struct V v;
    v.kind = 2;
    v.as.number = d;
    return v;
}

static struct V do_binary(int *which, int *op, struct V left, struct V right)
{
    if (*which != 0) return number(0.0);
    if (*op == '+') return number(left.as.number + right.as.number);
    return number(left.as.number - right.as.number);
}

int main(void)
{
    int which = 0;
    int op = '+';
    struct V a = number(40.0);
    struct V b = number(2.0);
    struct V r = do_binary(&which, &op, a, b);

    if (r.kind != 2) return 1;
    return (int)r.as.number;
}
