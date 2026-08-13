// expect: 1
/* A signed 3-bit field holding 7 reads back as -1: the top bit of the field is
   its sign bit, which is why extraction shifts rather than masks. */
struct F { int a : 3; unsigned int b : 3; };
int main(void)
{
    struct F f;
    f.a = 7;
    f.b = 7;
    return (f.a == -1) && (f.b == 7);
}
