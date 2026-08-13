// expect: 5
/* The use that actually matters: two counters walking towards each other. */
int main(void)
{
    int i;
    int j;
    int n = 0;
    for (i = 0, j = 9; i < j; ++i, --j)
        n = n + 1;
    return n;
}
