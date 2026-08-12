// expect: 7
/* a[i] is *(a+i), so i[a] is legal C */
int main(void) { int a[3]; a[2] = 7; return 2[a]; }
