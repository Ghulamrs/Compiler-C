// expect: 63
// Pointers, a global, an array, unsigned and signed comparison, break.
int counter;
int table[4];
int deref(int *p) { return *p; }
int main(void) {
    int x; int y; int *p; unsigned int u; long l; char c;
    x = 7; y = 3;
    p = &x;
    *p = *p + 1;
    counter = x * y;
    table[2] = counter - 4;
    u = 4294967295u;
    l = -1;
    c = 'A';
    if (x > y && y != 0) counter = counter + 1;
    while (y > 0) { y = y - 1; if (y == 1) break; }
    return (x + counter + table[2] + deref(&x) + (int)(u > 0) + (int)(l < 0) + (c - 'A')) % 200;
}
