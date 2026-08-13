// expect: 1
/* The arm not taken is not evaluated, which is the reason this is an operator
   and not a function. */
int calls = 0;
int bump(void) { calls = calls + 1; return 0; }
int main(void)
{
    int r = 1 ? 5 : bump();
    if (r != 5) return 100;
    return calls + 1;
}
