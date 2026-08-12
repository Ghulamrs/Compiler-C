// expect: 1
/* is 97 prime? */
int main(void)
{
    int n = 97;
    int d = 2;
    int isPrime = 1;
    while (d * d <= n) {
        if (n % d == 0) {
            isPrime = 0;
        }
        d = d + 1;
    }
    return isPrime;
}
