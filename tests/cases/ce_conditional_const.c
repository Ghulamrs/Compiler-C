// expect: 8
/* Only the arm taken has to fold, which is what makes this the idiom it is. */
int width = sizeof(long) == 8 ? 8 : 4;
int main(void)
{
    int a[sizeof(long) == 8 ? 8 : 4];
    return sizeof(a) / sizeof(int) + width - 8;
}
