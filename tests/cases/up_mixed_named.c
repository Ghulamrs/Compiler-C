// expect: 12
/* Some named, some not, in one prototype - C allows the mixture. */
int three(int, int b, int);
int three(int a, int b, int c) { return a + b + c; }
int main(void)
{
    return three(3, 4, 5);
}
