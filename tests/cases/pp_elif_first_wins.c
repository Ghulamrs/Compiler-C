// expect: 11
/* Only the first true arm is used, even when a later one is also true.

   Accumulating rather than returning on purpose: with a return in each arm the
   first one wins at run time whether or not the second was also emitted, so the
   test would pass on a preprocessor that emitted both. This one does not. */
#define N 5
int main(void)
{
    int r = 0;
#if N > 1
    r = r + 11;
#elif N > 2
    r = r + 22;
#else
    r = r + 33;
#endif
    return r;
}
