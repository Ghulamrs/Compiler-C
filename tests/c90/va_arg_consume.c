#include <stdarg.h>
int s(int n, ...){ va_list ap; int t=0,i; va_start(ap,n); for(i=0;i<n;i++) t+=va_arg(ap,int); va_end(ap); return t; }
int main(void){ return s(2,3,4)-7; }
