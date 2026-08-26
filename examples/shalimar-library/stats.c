/* A library for Shalimar programs to call, written in C89 and compiled by cc1.
 *
 * The only thing that makes it a Shalimar library rather than any other C is
 * the argument type: a Shalimar array arrives as a `ShmArray *`, which is an
 * OPAQUE handle. This file never learns how an array is stored - it asks
 * shm_array_dim how long one is and shm_get_real what is in it, and that is
 * the whole of the contract. See ../../../Compiler-S/docs/ARRAY-ABI.md.
 *
 * Two rules that are easy to get wrong:
 *
 *   1. **No main().** The Shalimar runtime owns main; a second one collides
 *      at the link and the message is about a duplicate symbol rather than
 *      about this file.
 *
 *   2. **A rank-2 array is nested.** shm_get_real on a real[][] reads a row
 *      REFERENCE as a double and hands back nonsense with no diagnostic. Get
 *      the row with shm_get_ref first - see mean_of_rows below.
 */
#include "shmrt.h"

/* rank 1: the elements are doubles and are read directly. */
double stats_total(const ShmArray *a)
{
    int32_t n = shm_array_dim(a, 0);
    double  sum = 0.0;
    int32_t i;
    for (i = 0; i < n; i++) sum += shm_get_real(a, i);
    return sum;
}

double stats_mean(const ShmArray *a)
{
    int32_t n = shm_array_dim(a, 0);
    return n > 0 ? stats_total(a) / n : 0.0;
}

double stats_largest(const ShmArray *a)
{
    int32_t n = shm_array_dim(a, 0);
    double  best;
    int32_t i;
    if (n <= 0) return 0.0;
    best = shm_get_real(a, 0);
    for (i = 1; i < n; i++) {
        double v = shm_get_real(a, i);
        if (v > best) best = v;
    }
    return best;
}

/* rank 2: the outer array holds references to rows. This is the one that
   catches people, so the library carries an example of doing it properly. */
double stats_grand_total(const ShmArray *a)
{
    int32_t rows = shm_array_dim(a, 0);
    int32_t cols = shm_array_dim(a, 1);
    double  sum = 0.0;
    int32_t r, c;
    for (r = 0; r < rows; r++) {
        ShmArray *row = shm_get_ref(a, r);      /* NOT shm_get_real */
        for (c = 0; c < cols; c++) sum += shm_get_real(row, c);
    }
    return sum;
}

/* Writing back through the handle: scale every element in place. Shalimar
   passes an array by reference, so the caller sees this. */
void stats_scale(ShmArray *a, double by)
{
    int32_t n = shm_array_dim(a, 0);
    int32_t i;
    for (i = 0; i < n; i++) shm_set_real(a, i, shm_get_real(a, i) * by);
}
