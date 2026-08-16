/* An ordinary C90 program, built inside Visual Studio by cc1 rather than cl.
 *
 * Nothing here is written for the shim. It is C90 using the headers cc1
 * ships, which is the whole of what a project has to keep to in order to
 * build this way - and the DEMO macro comes from the .vcxproj, to show that
 * /D reaches the compiler through the response file MSBuild writes.
 */
#include <stdio.h>
#include <setjmp.h>
#include <string.h>

static jmp_buf env;

struct Mixed {
    int    a;
    double d;
    char   s[8];
};

static void deeper(int n)
{
    char pad[64];
    int i;
    for (i = 0; i < 64; i++) pad[i] = (char)(n + i);
    if (n > 0) deeper(n - 1);
    longjmp(env, pad[0] == (char)n ? 5 : 99);
}

int main(void)
{
    struct Mixed m;
    long double third;
    int r;

    m.a = 42;
    m.d = 2.5;
    strcpy(m.s, "ok");
    third = 1.0L / 3.0L;

    printf("built by cc1 inside MSBuild\n");
#ifdef DEMO
    printf("DEMO reached the compiler: %d\n", DEMO);
#else
    printf("DEMO did not arrive\n");
#endif

    printf("data model: int=%d long=%d ptr=%d long double=%d\n",
           (int)sizeof(int), (int)sizeof(long), (int)sizeof(void *),
           (int)sizeof(long double));
    printf("struct: a=%d d=%.1f s=%s\n", m.a, m.d, m.s);
    printf("third=%.10Lf\n", third);

    /* The assignment form, across four frames of scribbled-on stack. */
    r = setjmp(env);
    if (r == 0) deeper(3);
    printf("longjmp gave %d\n", r);

    return (r == 5) ? 0 : 1;
}
