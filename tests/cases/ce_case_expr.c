// expect: 2
/* The thing that was refused by name until the evaluator existed. */
int main(void)
{
    switch (3) {
    case 1 + 1: return 1;
    case 1 + 2: return 2;
    default: return 3;
    }
}
