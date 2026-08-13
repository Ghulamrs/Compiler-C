// expect: 6
/* Through a pointer, which is the same node after the parser lowers p->a. */
struct F { unsigned int a : 3; unsigned int b : 3; };
int set(struct F *p) { p->a = 2; p->b = 4; return p->a + p->b; }
int main(void)
{
    struct F f;
    return set(&f);
}
