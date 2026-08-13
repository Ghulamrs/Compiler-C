// expect: 0
/* Two functions may each have a "static int n". They are two objects, and the
   assembler must not see one symbol twice.

   One call per statement on purpose: C leaves the order in which a call's
   arguments are evaluated unspecified, and this compiler does not choose the
   same order gcc does. A test that depends on it is testing nothing. */
int printf(char *fmt, ...);
int a(void) { static int n = 10; n = n + 1; return n; }
int b(void) { static int n = 20; n = n + 1; return n; }
int main(void)
{
    printf("%d ", a());
    printf("%d\n", a());
    printf("%d ", b());
    printf("%d\n", b());
    return 0;
}
