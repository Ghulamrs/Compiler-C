#include <stdio.h>

// Struct with explicit unused padding bits

int main(void)
{
    double y[2];
    double x[2] = {5.0, 200.002};
    char filename[128] = "/Users/g.r.akhtar/Documents/Claude/Compiler-C/examples/CC1Lab/test.dat";
    FILE *fp = fopen(filename, "rb");
    for(int i=0; i<10; i++) {
        x[0] += 5.0;
        x[1] *= 2.5;
        fread(y, sizeof(double), 2, fp);
        printf("x[%1d]-y[%1d] = %.7lf\t", i, i, x[0]-y[0]);
        printf("x[%1d]-y[%1d] = %.7lf\n", i, i, x[1]-y[1]);
    }
    
    fclose(fp);
    
    return 0;
}
