// expect: 5
/* A qualifier before the '*' belongs to the pointee and one after it belongs to
   the pointer, so 'const char *s' leaves s itself assignable. That is what makes
   a string walk possible, and it is what this compiler had backwards: the const
   from the specifiers was applied to the object whatever the declarator said, so
   every loop of this shape was refused and every 'char *const' was allowed. */
int len(const char *s)
{
    int n = 0;
    while (*s) {
        n = n + 1;
        s = s + 1;
    }
    return n;
}

int main(void)
{
    const char *p = "hello";
    int x = 1;
    int *const q = &x;

    *q = len(p);
    p = p + 1;
    return *q + (p[0] == 'e') - 1;
}
