// expect: 0
/* Dynamic memory needs no library work here: malloc arrives through an
   ordinary prototype, and the cast that makes it a matrix is the declarator. */
int printf(char *, ...);
void *malloc(long);
int main(void)
{
    int (*m)[4] = (int (*)[4])malloc(4 * 4 * 4);
    int i;
    int j;
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            m[i][j] = i * 10 + j;
    printf("%d %d %d\n", m[0][0], m[2][3], m[3][3]);
    return 0;
}
