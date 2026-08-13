// expect: 9
/* Precedence: this is x = (c ? 9 : 4), not (x = c) ? 9 : 4. */
int main(void)
{
    int c = 1;
    int x = 0;
    x = c ? 9 : 4;
    return x;
}
