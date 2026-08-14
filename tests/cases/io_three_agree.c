// expect: 0
// printf, sprintf and fprintf are one function with three destinations, and
// the thing worth checking is that they agree - same conversions, same count
// returned, same bytes. A backend that placed the variadic arguments slightly
// differently for one of them would still print something plausible from the
// other two.
//
// The double is what makes this more than a string test: it is the only
// argument here that travels in a different register file from the rest.
#include <stdio.h>
#include <string.h>

int main(void)
{
    char buf[128];
    int a, b, c;
    FILE *out = fopen("/dev/null", "w");

    a = printf( "int %d, double %f, string %s, char %c, hex %x\n",
                42, 3.14159, "world", 'Z', 48879);
    b = sprintf(buf,
                "int %d, double %f, string %s, char %c, hex %x\n",
                42, 3.14159, "world", 'Z', 48879);
    c = fprintf(out,
                "int %d, double %f, string %s, char %c, hex %x\n",
                42, 3.14159, "world", 'Z', 48879);
    fclose(out);

    printf("via sprintf: %s", buf);
    if (a != b) return 1;
    if (a != c) return 2;
    if ((int)strlen(buf) != a) return 3;
    return 0;
}
