// expect: 5
#define A 1
int main(void)
{
#if defined(A) && !defined(B)
    return 5;
#else
    return 6;
#endif
}
