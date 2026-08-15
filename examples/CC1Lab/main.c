/* A place to write C and find out what cc1 does with it.
 *
 * Build with Cmd-B. cc1 compiles this file; clang looks at it too and its
 * opinion appears in the log prefixed "clang says:". Where the two disagree is
 * a finding - either a gap in cc1 or a mistake in the program, and the two
 * verdicts side by side are what tell you which.
 *
 * Only the five headers cc1 ships are reachable: stdio.h, stdlib.h, string.h,
 * stddef.h, stdarg.h. There is no system include path, so the SDK is not on
 * the table.
 */
#include <stdio.h>

int main(void)
{
    int i;
    double total = 0.0;

    for (i = 1; i <= 15; i++) {
        total += 1.0 / i;
        printf("%d  %.5f\n", i, total);
    }

    /* Uncomment to watch cc1 refuse what C90 allows - brace elision, which is
       the largest single gap in tests/c90/:

       int grid[2][2] = {1, 2, 3, 4};
       printf("%d\n", grid[1][1]);
    */
    return 0;
}
