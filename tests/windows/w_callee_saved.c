// expect: 54
// forbid: %rdi
// forbid: %rsi
// Microsoft x64 makes %rdi and %rsi the callee's to give back, where System V
// makes them scratch. The generator's scratch register on this target is %r10
// for exactly that reason.
//
// Nothing that runs here can catch a breach: glibc calls main and this suite
// links no Windows caller that would notice its registers had been eaten. So
// the check is on the text rather than on the answer, and the case is written
// to be register-hungry - nested arithmetic on eight locals - so that a
// generator reaching for a scratch has plenty of chances to reach wrongly.
int main(void)
{
    int a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8;
    int r = (a + b) * (c + d) - (e + f) * (g - h) + (a * h) + (b * g);
    return r;
}
