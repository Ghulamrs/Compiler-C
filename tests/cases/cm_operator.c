// expect: 1
/* The discarded operand still runs - that is the whole point of the operator. */
int main(void)
{
    int x;
    int y;
    x = 0, y = 1;
    return x + y;
}
