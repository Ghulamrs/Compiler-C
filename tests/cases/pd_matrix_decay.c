// expect: 7
/* A matrix used as a value is a pointer to its first row, and that row type is
   what the declaration has to be able to say. */
int main(void)
{
    int a[2][3];
    int (*r)[3] = a;
    r[1][2] = 7;
    return a[1][2];
}
