// expect: 0
/* toolkit.c - a small numerical and string toolkit.
 *
 * Written for the subset this compiler accepts today, which shapes it in
 * visible ways: every loop is a while, because there is no for; every counter
 * advances with "i = i + 1", because there is no ++; and is_prime carries a
 * flag rather than returning early from inside its loop, because there is no
 * break.
 */

/* Prototypes come first. This compiler refuses an undeclared name rather than
   assuming it returns int, so libc needs declaring exactly as a header would. */
int printf(char *fmt, ...);
int putchar(int c);

int    is_prime(int n);
int    gcd(int a, int b);
long   factorial(int n);
double newton_sqrt(double x);
int    str_length(char *s);
int    str_copy_reversed(char *src, char *dst, int room);
int    sort(int *a, int n);
int    swap(int *a, int *b);
int    sum_digits(long n);
int    rule(int width);

/* File scope. 'static' keeps this one to this unit. */
static int primes_seen;
int scratch[16];

int swap(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
    return 0;
}

/* Bubble sort, through a pointer - the array decayed at the call. */
int sort(int *a, int n)
{
    int i = 0;
    while (i < n - 1) {
        int j = 0;
        while (j < n - 1 - i) {
            if (a[j] > a[j + 1]) { swap(&a[j], &a[j + 1]); }
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}

int is_prime(int n)
{
    int factor = 2;
    int prime = 1;
    if (n < 2) { prime = 0; }
    while (factor * factor <= n) {
        if (n % factor == 0) { prime = 0; }
        factor = factor + 1;
    }
    if (prime) { primes_seen = primes_seen + 1; }
    return prime;
}

int gcd(int a, int b)
{
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

/* long, so 20! does not overflow the way an int would */
long factorial(int n)
{
    if (n <= 1) { return 1; }
    return (long)n * factorial(n - 1);
}

/* Newton's method. Doubles throughout, and the loop ends on a floating
   comparison rather than a fixed count. */
double newton_sqrt(double x)
{
    if (x <= 0.0) { return 0.0; }

    double guess = x;
    double half = 0.5;
    int steps = 0;

    while (steps < 40) {
        double next = half * (guess + x / guess);
        double delta = next - guess;
        if (delta < 0.0) { delta = -delta; }
        guess = next;
        if (delta < 0.0000000001) { steps = 40; } else { steps = steps + 1; }
    }
    return guess;
}

int str_length(char *s)
{
    int n = 0;
    while (s[n]) { n = n + 1; }
    return n;
}

int str_copy_reversed(char *src, char *dst, int room)
{
    int n = str_length(src);
    if (n >= room) { return 0; }

    int i = 0;
    while (i < n) {
        dst[i] = src[n - 1 - i];
        i = i + 1;
    }
    dst[n] = 0;
    return n;
}

int sum_digits(long n)
{
    int total = 0;
    if (n < 0) { n = -n; }
    while (n > 0) {
        total = total + (int)(n % 10);
        n = n / 10;
    }
    return total;
}

int rule(int width)
{
    int i = 0;
    while (i < width) { putchar(45); i = i + 1; }
    putchar(10);
    return 0;
}

int main(void)
{
    /* Split in two: six integer arguments is all the registers hold, and the
       format string is one of them. */
    printf("sizes: char %d  short %d  int %d  long %d\n",
           (int)sizeof(char), (int)sizeof(short),
           (int)sizeof(int), (int)sizeof(long));
    printf("       float %d  double %d  pointer %d\n",
           (int)sizeof(float), (int)sizeof(double), (int)sizeof(int *));
    rule(60);

    /* Primes below 30, counted through a static global. */
    printf("primes: ");
    int n = 2;
    while (n < 30) {
        if (is_prime(n)) { printf("%d ", n); }
        n = n + 1;
    }
    printf("\n  %d of them\n", primes_seen);
    rule(60);

    /* An array, sorted in place through a pointer. */
    int data[8];
    data[0] = 42; data[1] = 7;  data[2] = 19; data[3] = 3;
    data[4] = 88; data[5] = 12; data[6] = 55; data[7] = 1;

    printf("before:");
    int i = 0;
    while (i < 8) { printf(" %d", data[i]); i = i + 1; }
    printf("\n");

    sort(data, 8);

    printf(" after:");
    i = 0;
    while (i < 8) { printf(" %d", data[i]); i = i + 1; }
    printf("\n  %d bytes, %d elements\n",
           (int)sizeof data, (int)(sizeof data / sizeof data[0]));
    rule(60);

    /* Doubles, and a table of square roots. */
    double v = 2.0;
    while (v <= 5.0) {
        double r = newton_sqrt(v);
        printf("  sqrt(%.1f) = %.8f   squared back: %.8f\n", v, r, r * r);
        v = v + 1.0;
    }
    rule(60);

    /* Strings through char arrays and pointers. */
    char *phrase = "a man a plan a canal panama";
    char buffer[32];
    int len = str_copy_reversed(phrase, buffer, (int)sizeof buffer);
    printf("  \"%s\"\n  \"%s\"\n  length %d, buffer %d\n",
           phrase, buffer, len, (int)sizeof buffer);
    rule(60);

    /* Integers wide and narrow, signed and not. */
    long f20 = factorial(20);
    printf("  20! = %ld, digits summing to %d\n", f20, sum_digits(f20));
    printf("  gcd(1071, 462) = %d\n", gcd(1071, 462));

    unsigned int u = 1;
    int neg = -1;
    printf("  -1 < 1u is %d   (the int converts to unsigned)\n", neg < u);
    printf("  -1 >> 1 is %d   and (unsigned)-1 >> 1 is %u\n",
           neg >> 1, ((unsigned int)neg) >> 1);

    char narrow = 300;
    printf("  char narrow = 300 reads back as %d\n", narrow);

    /* scratch is a global array; the local shadows nothing here. */
    scratch[0] = 1;
    scratch[15] = 15;
    printf("  global array: %d..%d over %d bytes\n",
           scratch[0], scratch[15], (int)sizeof scratch);

    return 0;
}
