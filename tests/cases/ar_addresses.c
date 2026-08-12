// expect: 1
/* The gap between consecutive elements is the element size. */
int main(void)
{
    int a[2]; char c[2];
    long ints = (long)&a[1] - (long)&a[0];
    long chars = (long)&c[1] - (long)&c[0];
    return (ints == 4) * (chars == 1);
}
