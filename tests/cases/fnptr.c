// expect: 0
// Pointers to functions, in the forms a program actually writes them: declared,
// assigned, reseated, passed as a parameter, held in an array, compared, and
// measured. The name of a function used as a value is a pointer to it with no
// '&' written, which is C's rule and the one that makes qsort(a, n, s, cmp)
// look the way it does.
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

double scale(double d) { return d * 2.5; }
int nothing(void)      { return 99; }

// A parameter of function-pointer type, which is how a program takes an
// operation rather than a value.
int apply(int (*op)(int, int), int a, int b) { return op(a, b); }

typedef int (*BinOp)(int, int);

int main(void) {
    int (*f)(int, int);
    double (*g)(double);
    int (*h)(void);
    BinOp table[3];
    int i;

    f = add;
    printf("direct : %d\n", f(3, 4));
    f = sub;
    printf("reseat : %d\n", f(10, 4));

    printf("param  : %d %d\n", apply(mul, 6, 7), apply(add, 1, 2));

    // Returning something that is not an int, so the result register is chosen
    // by the type behind the pointer rather than by luck.
    g = scale;
    printf("double : %.2f\n", g(2.0));

    h = nothing;
    printf("void   : %d\n", h());

    // An array of them, called by subscript - the call is on the result of the
    // postfix chain rather than on a name.
    table[0] = add;
    table[1] = sub;
    table[2] = mul;
    i = 0;
    while (i < 3) {
        printf("table%d : %d\n", i, table[i](12, 3));
        i = i + 1;
    }

    printf("compare: %d %d\n", f == sub, f == add);
    printf("size   : %d\n", (int)sizeof f);

    // A typedef of the type, initialised from a function name.
    {
        BinOp t = mul;
        printf("typedef: %d\n", t(5, 5));
    }
    return 0;
}
