// <memory.h>, which predates the standard and which cc1 did not ship.
//
// A program written before <string.h> existed, or against a System V or BSD
// system that kept both, includes this one for memset and its neighbours. cc1
// shipped fifteen headers and not this, so such a program was refused for
// being older than the header list rather than for anything it did.
//
// Both headers are included here on purpose: they must not clash, and the six
// functions are declared in exactly one place with the other forwarding.
#include <stdio.h>
#include <string.h>
#include <memory.h>

int main(void)
{
    char buf[8];
    char copy[8];

    memset(buf, 'x', 7);
    buf[7] = 0;
    printf("%s %d\n", buf, (int)strlen(buf));

    memcpy(copy, buf, 8);
    printf("%s %d\n", copy, memcmp(copy, buf, 8));

    memset(buf, 0, sizeof buf);
    printf("%d\n", (int)strlen(buf));

    return 0;
}
