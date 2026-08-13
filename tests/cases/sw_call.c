// expect: 2
int f(int x) { return x + 1; }
int main(void)
{
    switch (f(1)) {
    case 1: return 1;
    case 2: return 2;
    }
    return 0;
}
