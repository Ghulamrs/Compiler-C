// Bit-fields, which are placed by counting bits from the start of the whole
// object rather than by a byte offset and are the one member shape that can
// be described wrongly while every other one is right.
//
// stop: 27
// func: pack 23
// print: f.a 5
// print: f.b 21
// print: f.c -3
// print: f.wide 1000
// bt: pack main

struct Flags {
    unsigned int a : 3;
    unsigned int b : 5;
    int c : 4;
    unsigned int wide : 20;
};

int pack(void)
{
    struct Flags f;
    f.a = 5;
    f.b = 21;
    f.c = -3;
    f.wide = 1000;
    return f.a + f.b;
}

int main(void)
{
    return pack();
}
