// expect: 42
typedef int *IntPtr;
int main(void) { int v = 42; IntPtr p = &v; return *p; }
