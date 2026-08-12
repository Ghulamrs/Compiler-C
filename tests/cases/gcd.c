// expect: 6
/* Euclid, which is why % had to exist */
int main(void)
{
    int a = 48;
    int b = 18;
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}
