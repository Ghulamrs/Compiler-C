// expect: 4
/* The skipped arm is never handed to the compiler, so it need not even be
   valid C - which is the whole reason conditionals exist. */
int main(void)
{
#ifdef NEVER
    this is not C at all ((( ;
#endif
    return 4;
}
