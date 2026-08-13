// expect: 0
/* Only declarations - which is all a header would have given it. */
int printf(char *, ...);
int factorial(int);
long sum_to(int);

int main(void)
{
    printf("factorial(6) = %d\n", factorial(6));
    printf("sum_to(100)  = %ld\n", sum_to(100));
    return 0;
}
