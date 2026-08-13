// expect: 6
/* The declaration the parentheses exist for: p points at an array of three,
   not at three pointers. */
int main(void)
{
    int a[3];
    int (*p)[3] = &a;
    (*p)[1] = 6;
    return a[1];
}
