// expect: 0
#include "shape.h"
int printf(char *, ...);
int main(void)
{
    struct Box b;
    b.w = 3;
    b.h = 5;
    printf("area=%d sides=%d\n", area(&b), SIDES);
    printf("made=%d\n", boxes_made);
    return 0;
}
