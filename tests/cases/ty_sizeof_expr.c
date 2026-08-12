// expect: 1
int main(void) { char c; short s; long l;
                 return (sizeof c == 1) * (sizeof s == 2) * (sizeof l == 8); }
