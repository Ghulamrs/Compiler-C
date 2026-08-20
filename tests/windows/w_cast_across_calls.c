// expect: 1
// Casts where the value being converted has just travelled, or is about to.
// The conversions themselves are the same on every target and tests/cases pins
// them; what is Microsoft's here is where the operand comes from - %rcx %rdx
// %r8 %r9, then the stack above the shadow area - and whether a narrow type
// arrives and leaves intact when it is one of those.
//
// The last argument is the one that matters: it is the sixth, so it is passed
// on the stack, and it is a char being widened by the callee. A backend that
// read a stack argument at the wrong offset, or read four bytes of a one-byte
// slot, would come back with something other than 200 here and nowhere else.
unsigned char narrow(int i)
{
    return (unsigned char)i;
}

int widen(unsigned char c)
{
    return (int)c;
}

int sixth(int a, int b, int c, int d, int e, unsigned char f)
{
    return (int)(unsigned char)(a + b + c + d + e) * 1000 + (int)f;
}

int fromDouble(double d)
{
    return (int)d;
}

double toDouble(int i)
{
    return (double)i;
}

struct Mixed {
    unsigned char tag;
    int count;
};

int throughPointer(struct Mixed *m)
{
    return (int)(unsigned char)m->count;
}

int main(void)
{
    struct Mixed m;
    int ok = 1;

    m.tag = 200;
    m.count = 456;

    if (widen(narrow(456)) != 200) ok = 0;          /* out and back, both narrow */
    if ((int)(signed char)narrow(456) != -56) ok = 0;
    if (sixth(100, 100, 100, 100, 56, 200) != 200200) ok = 0;
    if (fromDouble(toDouble(456) / 100.0) != 4) ok = 0;
    if (throughPointer(&m) != 200) ok = 0;
    if ((unsigned char)widen(m.tag) != 200) ok = 0;

    return ok;
}
