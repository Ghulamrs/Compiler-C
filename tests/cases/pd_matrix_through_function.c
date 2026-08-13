// expect: 8
void *malloc(long);
int at(int (*m)[3], int i, int j) { return m[i][j]; }
int main(void)
{
    int (*m)[3] = (int (*)[3])malloc(36);
    m[2][1] = 8;
    return at(m, 2, 1);
}
