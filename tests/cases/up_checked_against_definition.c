// expect: 3
/* The prototype still checks the definition and the call, name or no name. */
long widen(int);
long widen(int n) { return (long)n + 1; }
int main(void)
{
    return (int)widen(2);
}
