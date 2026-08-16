// expect: 0
// Integers and doubles interleaved, which is the case the two-offset walk
// exists for. Under System V they are drawn from two different register files
// with two different offsets and two different limits, so taking an int must
// not move the floating cursor and taking a double must not move the integer
// one. A walk that keeps a single cursor gets the right answer for a call that
// is all one type and the wrong one here.
//
// Ten doubles and ten integers, so both files are exhausted and both spill.
#include <stdarg.h>

static double walk(int pairs, ...)
{
    va_list ap;
    double t = 0;
    int i;
    va_start(ap, pairs);
    for (i = 0; i < pairs; i++) {
        t += (double)va_arg(ap, int);
        t += va_arg(ap, double);
    }
    va_end(ap);
    return t;
}

int main(void)
{
    /* ten pairs: the integers sum to 55, the doubles to 5.0 */
    double got = walk(10,
                      1, 0.5, 2, 0.5, 3, 0.5, 4, 0.5, 5, 0.5,
                      6, 0.5, 7, 0.5, 8, 0.5, 9, 0.5, 10, 0.5);
    return (int)(got - 60.0);
}
