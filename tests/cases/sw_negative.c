// expect: 5
int main(void)
{
    int x = -2;
    switch (x) {
    case -1: return 1;
    case -2: return 5;
    default: return 9;
    }
}
