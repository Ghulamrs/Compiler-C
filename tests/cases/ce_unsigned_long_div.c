// expect: 6
/* The same distinction in division and in the shift: an unsigned long divided
   or shifted is not a negative long divided or shifted. */
int main(void)
{
    switch (0) {
    case (int)((0UL - 1UL) / 3UL): return 9;      /* huge, truncates to -1431655765 */
    case (int)((0UL - 1UL) >> 60): return 9;      /* 15 unsigned, -1 signed */
    default: return 6;
    }
}
