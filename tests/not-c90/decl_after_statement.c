/* C90 puts every declaration at the top of its block. C99 lifted that. */
int main(void)
{
    int a = 1;
    a = a + 1;
    int b = 2;
    return a + b - 4;
}
