// expect: 0
/* Casts, on the backend that has to narrow in a register rather than by
   choosing a smaller move: uxtb, sxtb and the rest of genConversion.
   tests/cases has these as three small cases; here they are one program that
   prints, because what this suite is asking is whether the arm64 lowering
   agrees with clang about the answers, not whether one exit code came back.
   The pointer half is here for a reason of its own - a cast between pointer
   types converts nothing at run time and decides how wide the next load is. */
int printf(const char *, ...);

struct Point {
    int x;
    int y;
};

struct Mixed {
    unsigned char tag;
    int count;
    double weight;
    struct Point where;
};

static struct Point store;

int main(void)
{
    int i = 456;
    int neg = -1;
    double d = 3.99;
    int word = 0x01020304;
    int pair[2];
    unsigned char *bytes;
    char *raw;
    struct Mixed m;
    struct Mixed *mp = &m;
    struct Point p;
    void *any;
    struct Point *back;

    /* narrowing and widening, as values */
    printf("uchar %d, schar %d, back %d\n",
           (int)(unsigned char)i, (int)(signed char)200, (int)(unsigned char)(signed char)200);
    printf("neg %d %d %u\n",
           (int)(unsigned char)neg, (int)(unsigned short)neg, (unsigned int)neg);
    printf("float %d %.2f\n", (int)d, (double)i / 100.0);

    /* the same in conditions, where the conversion decides a branch */
    printf("cond %d %d %d\n",
           (unsigned char)i == 200, (unsigned char)256 != 0, !(signed char)200 == 0);

    /* members: plain, through a pointer, and a member of a member */
    m.tag = 200;
    m.count = 456;
    m.weight = 3.75;
    m.where.x = 5;
    m.where.y = 262;
    printf("member %d %d %d %d %d\n",
           (int)(unsigned char)m.count, (int)(signed char)m.tag, (int)m.weight,
           (int)(unsigned char)mp->count, (int)(unsigned char)mp->where.y);

    /* pointers: a narrower read, a step in bytes, and a trip through void * */
    bytes = (unsigned char *)&word;
    pair[0] = 11;
    pair[1] = 22;
    raw = (char *)pair;
    p.x = 7;
    p.y = 9;
    any = (void *)&p;
    back = (struct Point *)any;
    store.x = 41;
    printf("pointer %d %d %d %d %d\n",
           (int)bytes[0], bytes[0] + bytes[1] + bytes[2] + bytes[3],
           *(int *)(raw + sizeof(int)), back->x * 10 + back->y,
           *(int *)(void *)&store);

    return 0;
}
