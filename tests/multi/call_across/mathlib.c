/* This file has no main and has never seen the other one. */
int factorial(int n)
{
    int result = 1;
    int i;
    for (i = 2; i <= n; ++i) result = result * i;
    return result;
}

long sum_to(int n)
{
    long total = 0;
    int i;
    for (i = 1; i <= n; ++i) total = total + i;
    return total;
}
