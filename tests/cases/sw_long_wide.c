// expect: 7
int main(void)
{
    long n = 5000000000;
    switch (n) {
    case 1: return 1;
    case 5000000000: return 7;
    default: return 2;
    }
}
