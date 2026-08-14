// expect: 66
// A struct of twelve bytes goes in two integer registers, and the second of
// them is only half full. Loading a partial eightbyte takes a widening move,
// which needs a scratch register, and the arguments are placed last to first -
// so by the time this one is loaded, the registers after it already hold their
// values. A scratch register that is also an argument register destroys one.
//
// %rcx was that scratch, and %rcx is the fourth integer argument here. The
// twenty below came out as three, and 66 as 49, with the struct and the count
// both correct - which is why this case fills all four registers rather than
// testing the struct alone. Nothing in the suite reached it before.
struct Q { int a; int b; int c; };

int f(struct Q q, int c, int d, int e)
{
    return q.a + q.b + q.c + c + d + e;
}

int main(void)
{
    struct Q q;
    q.a = 1; q.b = 2; q.c = 3;
    return f(q, 10, 20, 30);
}
