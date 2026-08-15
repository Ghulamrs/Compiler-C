// expect: 12
/* A function may be declared inside a block, which is how a file reaches one
   name from another unit without a header. STATUS.md claimed block 'extern'
   worked; it worked for objects only, and a function declaration there stopped
   at 'expected ;'. The block limits where the name is visible and nothing else -
   the linkage is external wherever it is written, with or without the keyword. */
int helper(int n);

int main(void)
{
    extern int helper(int n);
    int again(int n);
    int total;

    total = helper(5);
    total = total + again(1);
    return total;
}

int helper(int n)
{
    return n * 2;
}

int again(int n)
{
    return n + 1;
}
