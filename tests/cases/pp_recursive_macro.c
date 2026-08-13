// expect: 6
/* A macro is not replaced inside its own expansion, so this terminates. */
#define N (N_BASE + 1)
#define N_BASE 5
int main(void)
{
    return N;
}
