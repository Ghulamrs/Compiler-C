// expect: 1
/* break leaves the inner loop only */
int main(void) { int outer = 0; int inner = 0;
                 for (int i = 0; i < 3; i = i + 1) { outer = outer + 1;
                     for (int j = 0; j < 10; j = j + 1) { if (j == 2) break; inner = inner + 1; } }
                 return (outer == 3) * (inner == 6); }
