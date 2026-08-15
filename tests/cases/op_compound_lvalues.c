// expect: 1
// The same rewrite over every target shape that is not a bare name: a
// dereferenced sum, a subscripted pointer, a member of an array element,
// and a pointer target stepped by its own element size.
struct S { int a; double d; };

int main(void) { int arr[4]; int *p; struct S s[2]; int *q; char c[1];
                 int t1, t2, t3, t4;

                 arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4;
                 p = arr;
                 *(p + 2) += 10;
                 p[3] *= 5;
                 t1 = arr[2] == 13 && arr[3] == 20;

                 s[0].a = 4; s[0].d = 1.5;
                 s[1].a = 9; s[1].d = 2.5;
                 s[0].a += s[1].a;
                 s[1].d -= s[0].d;
                 t2 = s[0].a == 13 && s[1].d == 1.0;

                 q = arr;
                 q += 3;
                 t3 = *q == 20;

                 c[0] = 100; c[0] += 100;   /* narrows back to char */
                 t4 = c[0] == -56;

                 return t1 && t2 && t3 && t4; }
