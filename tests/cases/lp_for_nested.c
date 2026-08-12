// expect: 100
int main(void) { int n = 0; for (int i = 0; i < 10; i = i + 1)
                              for (int j = 0; j < 10; j = j + 1) n = n + 1;
                 return n; }
