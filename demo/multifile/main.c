// expect: 0
/* the other unit: it has never seen stack.c, only these declarations */
int printf(char *fmt, ...);
int push(int v);
int pop(void);
extern int depth;

int main(void)
{
    int i = 1;
    while (i <= 5) { push(i * i); i = i + 1; }
    printf("depth after pushes: %d\n", depth);

    printf("popped:");
    while (depth > 0) { printf(" %d", pop()); }
    printf("\ndepth after pops: %d\n", depth);
    return 0;
}
