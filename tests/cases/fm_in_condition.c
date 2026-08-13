// expect: 14
/* A function-like macro used in a #if, which the condition evaluator has to
   expand for itself. */
#define DOUBLE(x) ((x) * 2)
int main(void)
{
#if DOUBLE(7) == 14
    return 14;
#else
    return 0;
#endif
}
