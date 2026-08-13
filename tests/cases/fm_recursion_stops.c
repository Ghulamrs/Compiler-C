// expect: 5
/* A macro naming itself in its own body is not replaced there, so the f that
   survives is the real function of that name and this terminates.

   The function is defined before the macro on purpose: after the #define, the
   text "int f(int n)" is itself a call to the macro, and writing it there would
   mangle the definition - in this compiler and in gcc alike. */
int f(int n) { return n; }
#define f(x) f(x) + 1
int main(void)
{
    return f(4);
}
