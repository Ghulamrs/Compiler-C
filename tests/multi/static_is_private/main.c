// expect: 0
int printf(char *, ...);
int left_twice(void);
int right_twice(void);
int main(void)
{
    printf("%d %d\n", left_twice(), right_twice());
    return 0;
}
