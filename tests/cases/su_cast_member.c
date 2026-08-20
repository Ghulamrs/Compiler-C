// expect: 1
/* The same conversions again, this time where the thing being cast has to be
   fetched first: a member, a member reached through a pointer, and a member of
   a member. A cast of an lvalue and a cast of a variable are one rule, and
   this is where a backend that forgot to narrow after the load would show it. */
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

int main(void)
{
    struct Mixed m;
    struct Mixed *p = &m;
    int ok = 1;

    m.tag = 200;
    m.count = 456;
    m.weight = 3.75;
    m.where.x = 5;
    m.where.y = 262;             /* 262 & 0xff is 6 */

    if ((unsigned char)m.count != 200) ok = 0;
    if ((int)(signed char)m.tag != -56) ok = 0;
    if ((int)m.weight != 3) ok = 0;
    if ((double)m.count != 456.0) ok = 0;

    if ((unsigned char)p->count != 200) ok = 0;
    if ((unsigned char)p->where.y != 6) ok = 0;
    if ((unsigned char)m.where.y != 6) ok = 0;

    return ok;
}
