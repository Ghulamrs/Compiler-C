// expect: 30
// Floating arguments interleaved with integer ones. Under System V these fill
// %xmm0 upwards independently of the integer registers; here each takes the
// slot of its own position, so b is in %xmm1 and d in %xmm3 and the two files
// never both hold a slot number.
int mix(int a, double b, int c, double d, int e)
{
    return a + (int)b + c + (int)d + e;
}

int main(void) { return mix(2, 4.0, 6, 8.0, 10); }
