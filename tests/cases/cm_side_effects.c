// expect: 6
/* Every operand is evaluated, left to right, and only the last one is kept. */
int calls = 0;
int bump(void) { calls = calls + 1; return 0; }
int main(void)
{
    int v = (bump(), bump(), bump(), 3);
    return v + calls;
}
