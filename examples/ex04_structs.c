// struct, union, enum and typedef, with C's layout rules, self-reference
// through a pointer, and a list built in a static pool.
#include <stdio.h>
#include "examples.h"

enum Colour { Red, Green = 5, Blue };

typedef struct Point { int x; int y; } Point;

struct Padded { char c; int n; char d; double f; };

union Word { unsigned int u; unsigned char b[4]; };

struct Node { int value; struct Node *next; };

static struct Node pool[4];

int ex_structs(void) {
    Point p;
    struct Padded pad;
    union Word w;
    struct Node *head;
    int i;

    printf("[structs]\n");
    p.x = 3; p.y = 4;
    printf("  point    : %d %d size=%d\n", p.x, p.y, (int)sizeof p);

    pad.c = 'a'; pad.n = 7; pad.d = 'b'; pad.f = 1.5;
    printf("  padded   : %d %d %d %.1f size=%d\n", pad.c, pad.n, pad.d, pad.f,
           (int)sizeof pad);

    w.u = 0x01020304u;
    printf("  union    : %u -> %u %u %u\n", w.u, w.b[0], w.b[1], w.b[2]);
    printf("             top byte %u, size %d\n", w.b[3], (int)sizeof w);

    printf("  enum     : %d %d %d\n", Red, Green, Blue);

    // A list, in a pool because this example does not allocate.
    for (i = 0; i < 4; i = i + 1) {
        pool[i].value = (i + 1) * 10;
        pool[i].next = i < 3 ? &pool[i + 1] : 0;
    }
    head = &pool[0];
    printf("  list     :");
    while (head != 0) { printf(" %d", head->value); head = head->next; }
    printf("\n");

    // Whole-object assignment, and the arrow as (*p).m
    {
        Point q;
        q = p;
        printf("  assigned : %d %d\n", q.x, q.y);
        printf("  arrow    : %d\n", (&q)->x);
    }
    return 8;
}
