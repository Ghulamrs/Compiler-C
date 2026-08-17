// expect: 0
/* Character constants at their edges: C90 6.1.3.4 allows more than one
   character with an implementation-defined value - this compiler takes
   gcc's, each byte shifted under the next - and a hex escape is as wide
   as the constant carrying it, so L'A41' keeps both bytes where a
   mask to char quietly kept one. '\377' stays -1: a single narrow
   constant takes char's signedness, a multi-character one is an int. */
#include <stdio.h>
int main(void) {
    printf("%d\n", 'ab');
    printf("%d\n", 'abcd');
    printf("%d\n", L'\x41');
    printf("%d\n", (int)L'\x4141');
    printf("%d\n", '\377');
    return 0;
}
