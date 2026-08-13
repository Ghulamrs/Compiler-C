// expect: 1
/* Without the parentheses the suffix binds tighter and it is an array of
   pointers, which is the thing the parentheses undo. */
int main(void)
{
    int *p[4];
    int a = 1;
    p[0] = &a;
    return *p[0];
}
