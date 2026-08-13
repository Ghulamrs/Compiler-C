// expect: 65
/* A const parameter is read-only; a const pointee is not carried, which is
   written down in docs/STATUS.md rather than pretended about. */
int first(const char *s) { return s[0]; }
int twice(const int n) { return n; }
int main(void)
{
    return first("A") + twice(0);
}
