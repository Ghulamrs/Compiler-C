/* Integers and integer arrays across the boundary.
 *
 * `int` in Shalimar is 32 bits and arrives as int32_t, so an ordinary C `int`
 * matches on all three targets. An `int[]` is a handle like any other array -
 * shm_get_int and shm_set_int rather than the _real pair, and shm_array_dim
 * for the length whatever the element type.
 *
 * Nothing here returns a real. The point of this file is that the boundary is
 * not a floating-point one: a library of integer work needs no doubles.
 */
#include "shmrt.h"

/* scalar in, scalar out */
int tally_gcd(int a, int b)
{
    while (b != 0) { int t = a % b; a = b; b = t; }
    return a < 0 ? -a : a;
}

/* an int[] read */
int tally_sum(const ShmArray *a)
{
    int32_t n = shm_array_dim(a, 0), i, s = 0;
    for (i = 0; i < n; i++) s += shm_get_int(a, i);
    return s;
}

int tally_count_over(const ShmArray *a, int threshold)
{
    int32_t n = shm_array_dim(a, 0), i, c = 0;
    for (i = 0; i < n; i++) if (shm_get_int(a, i) > threshold) c++;
    return c;
}

/* an int[] written through - Shalimar passes arrays by reference, so the
   caller sees this. Sorting in place is the clearest demonstration. */
void tally_sort(ShmArray *a)
{
    int32_t n = shm_array_dim(a, 0), i, j;
    for (i = 1; i < n; i++) {
        int32_t v = shm_get_int(a, i);
        j = i - 1;
        while (j >= 0 && shm_get_int(a, j) > v) {
            shm_set_int(a, j + 1, shm_get_int(a, j));
            j--;
        }
        shm_set_int(a, j + 1, v);
    }
}

/* a rank-2 int[][]: the outer array holds row references, exactly as for
   reals. The element accessor is the only thing that changes. */
int tally_diagonal(const ShmArray *m)
{
    int32_t rows = shm_array_dim(m, 0);
    int32_t cols = shm_array_dim(m, 1);
    int32_t i, s = 0;
    for (i = 0; i < rows && i < cols; i++) {
        ShmArray *row = shm_get_ref(m, i);
        s += shm_get_int(row, i);
    }
    return s;
}
