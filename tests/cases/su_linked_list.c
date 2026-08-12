// expect: 100
/* a list built in a static pool, since there is no malloc: 10+20+30+40 */
struct Node { int value; struct Node *next; };
static struct Node pool[8];
int main(void)
{
    int i = 0;
    while (i < 4) { pool[i].value = (i + 1) * 10; pool[i].next = &pool[i + 1]; i = i + 1; }
    pool[3].next = 0;

    int total = 0;
    struct Node *p = &pool[0];
    while (p != 0) { total = total + p->value; p = p->next; }
    return total;
}
