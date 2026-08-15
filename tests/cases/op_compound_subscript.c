// expect: 1
// A compound assignment is rewritten as 'x = x op e', which needs a second
// copy of the target. A subscript is '*(x + i)', so the copy has to reach
// through a '*' over an addition - the shape these two loops are built from.
int main(void) { int A[2][2]; int b[2]; int x[2];
                 int a[2][3]; int i, j, k, r; int t1, t2;

                 A[0][0] = 1; A[0][1] = 2;
                 A[1][0] = 3; A[1][1] = 4;
                 b[0] = 5; b[1] = 6;
                 x[0] = 0; x[1] = 0;
                 for (i = 0; i < 2; i++)
                     for (j = 0; j < 2; j++)
                         x[i] += A[i][j] * b[j];
                 t1 = x[0] == 17 && x[1] == 39;

                 a[0][0] = 2; a[0][1] = 4; a[0][2] = 6;
                 a[1][0] = 6; a[1][1] = 5; a[1][2] = 4;
                 i = 0; k = 1; r = a[k][i] / a[i][i];
                 for (j = 0; j < 3; j++)
                     a[k][j] -= r * a[i][j];
                 t2 = a[1][0] == 0 && a[1][1] == -7 && a[1][2] == -14;

                 return t1 && t2; }
