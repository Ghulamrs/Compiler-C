// Array and struct initialisers, at both storage durations: counted lengths,
// short lists that zero what they do not reach, strings into char arrays, and
// aggregates nested inside each other.
#include <stdio.h>
#include "examples.h"

struct Point { int x; int y; };
struct Frame { struct Point origin; int sides[4]; char name[8]; };

static int   table[]     = {2, 3, 5, 7, 11};
static char  greeting[]  = "hello";
static struct Point unit = {1, 1};
static struct Point box[2] = {{0, 0}, {4, 3}};
static int   grid[2][3]  = {{1, 2, 3}, {4, 5, 6}};
static int   sparse[6]   = {9, 8};

int ex_initialisers(void) {
    int local[] = {10, 20, 30};
    int partial[5] = {1, 2};
    char word[8] = "hi";
    struct Point p = {7, 8};
    struct Frame frame = {{1, 2}, {3, 4, 5, 6}, "edge"};
    struct Point zeroed = {0};
    int i, j;

    printf("[initialisers]\n");
    printf("  counted  : %d elements, %d bytes\n",
           (int)(sizeof table / sizeof table[0]), (int)sizeof table);
    printf("  string   : %s (%d bytes)\n", greeting, (int)sizeof greeting);
    printf("  local    : %d %d %d\n", local[0], local[1], local[2]);
    printf("  partial  : %d %d %d %d %d\n", partial[0], partial[1], partial[2],
           partial[3], partial[4]);
    printf("  padded   : %s last=%d\n", word, word[7]);
    printf("  struct   : %d %d\n", p.x, p.y);
    printf("  zeroed   : %d %d\n", zeroed.x, zeroed.y);
    printf("  nested   : %d %d | %d %d\n", frame.origin.x, frame.origin.y,
           frame.sides[0], frame.sides[1]);
    printf("             %d %d | %s\n", frame.sides[2], frame.sides[3],
           frame.name);
    printf("  of struct: %d %d %d %d\n", box[0].x, box[0].y, box[1].x, box[1].y);
    printf("  grid     :");
    for (i = 0; i < 2; i = i + 1)
        for (j = 0; j < 3; j = j + 1) printf(" %d", grid[i][j]);
    printf("\n");
    printf("  sparse   : %d %d %d\n", sparse[0], sparse[1], sparse[5]);
    printf("  unit     : %d %d\n", unit.x, unit.y);
    return 11;
}
