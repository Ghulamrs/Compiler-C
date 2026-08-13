// expect: 3
/* The storage outlives the call, which is the whole of what static means here.
   An ordinary local would start at 0 every time. */
int next(void)
{
    static int n = 0;
    n = n + 1;
    return n;
}
int main(void)
{
    next();
    next();
    return next();
}
