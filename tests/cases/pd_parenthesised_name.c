// expect: 3
/* Parentheses that undo nothing are still legal, and are not a function
   pointer. */
int (f)(void);
int (f)(void) { return 3; }
int main(void)
{
    return f();
}
