/* Text across the boundary.
 *
 * Shalimar has no string type: text is a `char[]`, and a char is a 32-bit
 * code rather than a byte - shm_get_char answers int32_t. So a C function
 * reading Shalimar text walks codes, not a `char *`, and there is no way to
 * hand one to strlen or printf. That is the boundary doing its job rather
 * than getting in the way: Shalimar has no pointer type, so no function
 * taking `char *` could have been declared in the first place.
 *
 * **A literal carries a terminator.** `char w[6] : "hello"` is six elements -
 * five codes and a nought - so a C function must stop at the nought rather
 * than trusting the extent, which is the array's size and not the text's
 * length. text_length below is the difference.
 */
#include "shmrt.h"

/* up to the terminator, which is not the same as shm_array_dim */
int text_length(const ShmArray *s)
{
    int32_t n = shm_array_dim(s, 0), i;
    for (i = 0; i < n; i++) if (shm_get_char(s, i) == 0) return i;
    return n;
}

int text_count(const ShmArray *s, int code)
{
    int32_t n = text_length(s), i, c = 0;
    for (i = 0; i < n; i++) if (shm_get_char(s, i) == code) c++;
    return c;
}

/* 1 if the two hold the same text. Shalimar's own `=` on two char[] compares
   addresses, which is why this is worth having as a function. */
int text_same(const ShmArray *a, const ShmArray *b)
{
    int32_t n = text_length(a), i;
    if (n != text_length(b)) return 0;
    for (i = 0; i < n; i++)
        if (shm_get_char(a, i) != shm_get_char(b, i)) return 0;
    return 1;
}

/* written through: upper-case in place, ASCII only, terminator preserved */
void text_upper(ShmArray *s)
{
    int32_t n = text_length(s), i;
    for (i = 0; i < n; i++) {
        int32_t c = shm_get_char(s, i);
        if (c >= 'a' && c <= 'z') shm_set_char(s, i, c - 'a' + 'A');
    }
}

/* Rotate the letters by `by`, in place, leaving everything else alone. The
   caller is expected to have reduced `by` into 0..25 already - Shalimar's own
   `fmod` does that, and doing it there rather than here is what lets the
   example show a borrowed table function feeding a borrowed library one. */
void text_shift(ShmArray *s, int by)
{
    int32_t n = text_length(s), i;
    for (i = 0; i < n; i++) {
        int32_t c = shm_get_char(s, i);
        if (c >= 'a' && c <= 'z') shm_set_char(s, i, 'a' + (c - 'a' + by) % 26);
        else if (c >= 'A' && c <= 'Z') shm_set_char(s, i, 'A' + (c - 'A' + by) % 26);
    }
}

/* Both types at once: how many characters of `s` are the digit `d` names,
   answering a real so the mix is visible in one signature. */
double text_fraction(const ShmArray *s, int code)
{
    int32_t n = text_length(s);
    return n > 0 ? (double)text_count(s, code) / n : 0.0;
}
