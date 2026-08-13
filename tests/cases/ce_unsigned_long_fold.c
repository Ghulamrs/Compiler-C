// expect: 4
/* Where the fold's signedness becomes observable. An unsigned int always lands
   in the folder as a non-negative long, so signed and unsigned comparisons
   agree on it - at 64 bits they do not. -1L converted to unsigned long has its
   top bit set, so this comparison is false where a signed one would be true. */
int main(void)
{
    switch (0) {
    case (-1L < 1UL): return 4;
    default: return 9;
    }
}
