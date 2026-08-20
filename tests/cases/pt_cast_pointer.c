// expect: 1
/* Casts between pointer types, which convert nothing at run time and decide
   everything about what happens next: how wide the read is, how far a step
   goes, and whether the value survives a trip out to void * and back.
   Reading the low byte first assumes a little-endian target, which all three
   of these are; the sum of the four bytes is there because it does not. */
#include <stddef.h>

struct Point {
    int x;
    int y;
};

static struct Point store;

int main(void)
{
    int i = 0x01020304;
    int pair[2];
    unsigned char *bytes;
    char *raw;
    int *second;
    void *any;
    struct Point p;
    struct Point *back;
    int ok = 1;

    bytes = (unsigned char *)&i;
    if (bytes[0] + bytes[1] + bytes[2] + bytes[3] != 10) ok = 0;
    if (bytes[0] != 4) ok = 0;

    pair[0] = 11;
    pair[1] = 22;
    raw = (char *)pair;
    second = (int *)(raw + sizeof(int));
    if (*second != 22) ok = 0;

    p.x = 7;
    p.y = 9;
    any = (void *)&p;
    back = (struct Point *)any;
    if (back->x != 7 || back->y != 9) ok = 0;

    /* out to a number and back, in a type wide enough to hold one */
    if ((struct Point *)(size_t)&store != &store) ok = 0;

    /* the first member reached through the struct's own address */
    store.x = 41;
    store.y = 42;
    if (*(int *)(void *)&store != 41) ok = 0;

    /* and a whole struct written through a cast pointer */
    *(struct Point *)(void *)&store = p;
    if (store.x != 7 || store.y != 9) ok = 0;

    return ok;
}
