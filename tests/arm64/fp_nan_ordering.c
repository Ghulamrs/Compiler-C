// expect: 0
/* IEEE says every comparison against NaN is false except !=, and the x86
   trap is that ucomis sets ZF, PF and CF all to one when either operand is
   NaN. So sete reads "equal" and setb reads "less" for a value that is
   neither, and this file used to print 1 for nan == nan.
 
   seta and setae need CF clear, which unordered never gives, so > and >= were
   already right - which is why only three of the nine lines were wrong, and
   why a spot check of one comparison would have missed it. < and <= are done
   by comparing the other way round and using those; == and != have to consult
   PF directly.
 
   The same trap appears twice more, in "if (nan)" and "!nan", because both
   compare against zero the same way. NaN is not equal to zero, so it is true.
 
   NaN and infinity have no literal syntax in C90 - INFINITY and NAN are C99
   macros in <math.h>, which this compiler does not ship - so they are made
   here by arithmetic, which is how C90 has always had to do it. */
int printf(const char *, ...);

int main(void)
{
    double zero = 0.0, one = 1.0, neg = -1.0;
    double inf  = one / zero;
    double ninf = neg / zero;
    double nan  = zero / zero;

    printf("inf  = %f\n", inf);
    printf("-inf = %f\n", ninf);
    printf("nan  = %f\n", nan);

    printf("inf > 1e300      : %d (want 1)\n", inf > 1e300);
    printf("ninf < -1e300    : %d (want 1)\n", ninf < -1e300);
    printf("inf + 1 == inf   : %d (want 1)\n", (inf + one) == inf);

    /* The trap: ucomisd sets ZF, PF and CF all for an unordered compare, so a
       bare sete after it calls NaN equal to itself. */
    printf("nan == nan       : %d (want 0)\n", nan == nan);
    printf("nan != nan       : %d (want 1)\n", nan != nan);
    printf("nan <  1.0       : %d (want 0)\n", nan < one);
    printf("nan >  1.0       : %d (want 0)\n", nan > one);
    printf("nan >= nan       : %d (want 0)\n", nan >= nan);
    printf("if (nan)         : %d (want 1)\n", nan ? 1 : 0);
    printf("!nan             : %d (want 0)\n", !nan);
    printf("if (0.0)         : %d (want 0)\n", zero ? 1 : 0);
    printf("!0.0             : %d (want 1)\n", !zero);
    return 0;
}
