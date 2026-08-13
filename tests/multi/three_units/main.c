// expect: 0
int printf(char *, ...);
int square_plus(int);
int via_b(int);
int main(void)
{
    printf("%d %d\n", square_plus(4), via_b(5));
    return 0;
}
