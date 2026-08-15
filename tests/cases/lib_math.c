// expect: 1
// <math.h>, which is prototypes here and the host's libm at link time. Every
// C90 function in it is called once, and the three that answer through a
// pointer - frexp, ldexp, modf - are checked as the round trip they form.
//
// Compared against exact values only where binary floating point has one:
// sqrt(4), pow(2,10) and the integral parts are exact, so '==' is honest.
// Where it is not - exp, log, the trig - the test is a tolerance, which is
// what the reference compiler's own answer is being held to as well.
#include <math.h>

static int nearly(double a, double b)
{
    double d = a - b;
    if (d < 0) d = -d;
    return d < 1e-9;
}

int main(void)
{
    double ip, fr, back;
    int e;
    int t1, t2, t3, t4, t5, t6;

    t1 = sqrt(4.0) == 2.0 && pow(2.0, 10.0) == 1024.0
      && fabs(-3.5) == 3.5 && fmod(7.0, 3.0) == 1.0;

    t2 = floor(-2.5) == -3.0 && ceil(-2.5) == -2.0
      && floor(2.5) == 2.0 && ceil(2.5) == 3.0;

    t3 = nearly(log(exp(2.0)), 2.0) && nearly(log10(1000.0), 3.0)
      && nearly(exp(0.0), 1.0);

    t4 = nearly(sin(0.0), 0.0) && nearly(cos(0.0), 1.0) && nearly(tan(0.0), 0.0)
      && nearly(asin(0.0), 0.0) && nearly(acos(1.0), 0.0) && nearly(atan(0.0), 0.0)
      && nearly(sinh(0.0), 0.0) && nearly(cosh(0.0), 1.0) && nearly(tanh(0.0), 0.0);

    /* atan2(1,1) is a quarter of pi, and four of them make it whole */
    t5 = nearly(4.0 * atan2(1.0, 1.0), 3.14159265358979311600);

    /* frexp splits, ldexp puts back; modf splits the other way */
    fr = frexp(12.0, &e);
    back = ldexp(fr, e);
    t6 = fr == 0.75 && e == 4 && back == 12.0;
    fr = modf(3.75, &ip);
    t6 = t6 && ip == 3.0 && fr == 0.75;

    return t1 && t2 && t3 && t4 && t5 && t6;
}
