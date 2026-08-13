// expect: 3
int main(void)
{
    int r = 0;
    int a = 1;
    int b = 2;
    switch (a) {
    case 1:
        switch (b) {
        case 2: r = r + 3; break;
        default: r = r + 100; break;
        }
        break;
    default:
        r = 50;
    }
    return r;
}
