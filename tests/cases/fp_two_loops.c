// expect: 0
/* The program that drove a week of work, kept as it was written. It arrived as
   test.c from Xcode, and in turn asked for: floating point and postfix on
   arm64-darwin, which it was refused twice over; the host as the default
   target, because its assembly would not assemble on the Mac that wrote it;
   and the driver's link step, so one command gives an answer.

   What it exercises: a float accumulating against two doubles, so every
   'x += 1.0' widens, adds and narrows; both ++ forms, prefix used for its
   value in the while condition; i carried from one loop into the next; and
   printf taking three floating arguments through the variadic boundary,
   twice per lap. */
#include <stdio.h>

int main(int argc, char *argv[])
{
    int i, j, k;
    int count = 0;
    float  x = 0.0f;
    double y = 0.0;
    double z = 0;

    printf("Hello world!\n");
    j = k = 0;
    for(i=0; i<10; i++) {
        count = count + 1;
        k++;
        j += k;
        x += 1.0;
        y += 2.0;
        z += x + y;
        printf("%2d. %5.1f %5.1f %7.1lf\n", count, x, y, z);
    }
    printf("\n%d\n", j);

    while(++i < 20) {
        count = count + 1;
        k++;
        j += k;
        x += 1.0;
        y += 2.0;
        z += x + y;
        printf("%2d. %5.1f %5.1f %7.1lf\n", count, x, y, z);
    }
    printf("\n%d\n", j);
    return 0;
}
