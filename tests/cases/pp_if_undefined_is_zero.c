// expect: 8
/* A name that was never defined is 0 in a condition. That is C's rule, and it
   is why "#if NOPE" is false rather than an error. */
int main(void)
{
#if NOPE
    return 1;
#else
    return 8;
#endif
}
