// expect: 1
/* The controlling expression is evaluated once, however many comparisons the
   chain below it makes. */
int calls = 0;
int bump(void) { calls = calls + 1; return 2; }
int main(void)
{
    switch (bump()) {
    case 1: break;
    case 2: break;
    default: break;
    }
    return calls;
}
