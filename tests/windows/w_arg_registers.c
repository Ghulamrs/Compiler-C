// expect: 21
// Four arguments in %rcx %rdx %r8 %r9 and the rest on the stack - where "the
// rest" starts 32 bytes higher than System V would put it, because the caller
// has left the callee a shadow area to spill into.
int add6(int a, int b, int c, int d, int e, int f)
{
    return a + b + c + d + e + f;
}

int main(void) { return add6(1, 2, 3, 4, 5, 6); }
