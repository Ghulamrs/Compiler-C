// expect: 0
// Struct members on arm64, which this backend refused by name until the member
// selection learned to compute an address. That one gap accounted for 31 of
// its 44 refusals, because '&' is not the only thing that needs an address -
// reading 's.n' does, and so does every bit-field, every '->' and every whole
// struct assignment.
//
// What is exercised: a member read and written, nesting, a member of an array
// element, the arrow form through a pointer, whole-struct copy by assignment,
// a union seen two ways, and bit-fields signed and unsigned including one that
// straddles nothing and one packed beside its neighbours.
int printf(const char *fmt, ...);

struct Point { int x; int y; };
struct Line  { struct Point a; struct Point b; int tag; };
struct Bits  { unsigned f : 5; unsigned g : 3; int s : 4; };
union  Both  { int i; unsigned u; };

int main(void)
{
    struct Point p;
    struct Line  l;
    struct Point row[3];
    struct Point *pp;
    struct Line  copy;
    struct Bits  bf;
    union  Both  u;
    int i, bad;

    bad = 0;

    /* a member written and read back */
    p.x = 3; p.y = 4;
    if (p.x != 3 || p.y != 4) bad = bad + 1;

    /* nested, two levels down */
    l.a.x = 1; l.a.y = 2; l.b.x = 5; l.b.y = 6; l.tag = 9;
    if (l.a.x != 1 || l.b.y != 6 || l.tag != 9) bad = bad + 2;

    /* a member of an array element, and the address arithmetic under it */
    for (i = 0; i < 3; i++) { row[i].x = i; row[i].y = i * 10; }
    if (row[0].x != 0 || row[2].x != 2 || row[2].y != 20) bad = bad + 4;

    /* through a pointer, both spellings */
    pp = &p;
    pp->x = 7;
    (*pp).y = 8;
    if (p.x != 7 || p.y != 8) bad = bad + 8;
    if (&pp->y != &p.y) bad = bad + 16;

    /* assignment copies the whole struct, nested members and all */
    copy = l;
    if (copy.a.x != 1 || copy.b.y != 6 || copy.tag != 9) bad = bad + 32;
    copy.a.x = 99;
    if (l.a.x != 1) bad = bad + 64;     /* a copy, not an alias */

    /* a union is one store seen two ways */
    u.i = -1;
    if (u.u != 4294967295u) bad = bad + 128;

    /* bit-fields: packed neighbours, and a signed one that must sign-extend */
    bf.f = 31; bf.g = 5; bf.s = -3;
    if (bf.f != 31 || bf.g != 5 || bf.s != -3) bad = bad + 256;
    bf.f = 1;
    if (bf.f != 1 || bf.g != 5 || bf.s != -3) bad = bad + 512;  /* no bleed */
    /* the value of a bit-field assignment is the field as it now reads */
    if ((bf.g = 9) != 1) bad = bad + 1024;   /* 9 is 1001b, three bits keep 1 */

    /* compound assignment reaches through a member the same way */
    p.x += 5;
    row[1].y -= 3;
    if (p.x != 12 || row[1].y != 7) bad = bad + 2048;

    if (bad != 0) printf("structs: %d\n", bad);
    return bad;
}
