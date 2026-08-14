// expect: 61
// A call inside the argument list of another call. Each opens its own shadow
// area and each has to hand the next one a stack still aligned to sixteen -
// the inner call happens while the outer one is part-way through building its
// arguments, so an error in either compounds rather than cancels.
int inner(int a, int b, int c, int d, int e) { return a + b + c + d + e; }
int outer(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }

int main(void)
{
    return outer(inner(1, 2, 3, 4, 5),
                 inner(1, 1, 1, 1, 1),
                 inner(2, 2, 2, 2, 2),
                 inner(0, 0, 0, 0, 5),
                 inner(1, 2, 3, 0, 0),
                 inner(4, 4, 4, 4, 4));
}
