// expect: 42
// C90 6.1.2.1: a typedef declared in a block belongs to that block. It hides an
// outer one for the rest of the block, and it is gone when the block closes.
// Both halves were wrong while typedefs_ was one flat map: the inner T was
// reported as "typedefed twice", and a block's typedef stayed visible after it.
int main(void)
{
    typedef int T;              /* T is int here */
    T outer = 40;
    {
        typedef char T;         /* hides the int one for this block only */
        T inner = 2;
        outer = outer + inner;
    }
    {
        typedef long T;         /* a sibling block may reuse the name */
        T again = 0;
        outer = outer + (int)again;
    }
    return (int)(outer + (T)0); /* T is int again out here */
}
