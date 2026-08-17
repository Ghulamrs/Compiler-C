#include <stdio.h>

/* Read twenty doubles back off disk and check they survived the round trip.
 *
 * Every difference printed should be 0.0000000: test.dat holds exactly the
 * values the loop computes, so a non-zero column means fread, the double
 * arithmetic or printf's %lf has drifted.
 *
 * The file is found relative to the working directory, or named as argv[1].
 * It used to be an absolute path naming one particular Mac, so the example ran
 * on that machine and nowhere else - and crashed rather than said so, because
 * nothing checked fopen. Both matter, because examples/ is a test surface and
 * gets run on all three machines. */

int main(int argc, char **argv)
{
    const char *filename = argc > 1 ? argv[1] : "test.dat";
    double y[2];
    double x[2] = {5.0, 200.002};
    FILE *fp = fopen(filename, "rb");
    int i;

    if (fp == NULL) {
        fprintf(stderr, "cannot open %s - run this from the directory holding"
                        " test.dat, or pass its path\n", filename);
        return 1;
    }

    for (i = 0; i < 10; i++) {
        x[0] += 5.0;
        x[1] *= 2.5;
        if (fread(y, sizeof(double), 2, fp) != 2) {
            fprintf(stderr, "%s ended after %d of the 10 pairs\n", filename, i);
            fclose(fp);
            return 1;
        }
        printf("x[%1d]-y[%1d] = %.7lf\t", i, i, x[0] - y[0]);
        printf("x[%1d]-y[%1d] = %.7lf\n", i, i, x[1] - y[1]);
    }

    fclose(fp);

    return 0;
}
