// expect: 6
/* The same distinction in division and in the shift: an unsigned long divided
   or shifted is not a negative long divided or shifted.

   This case assumes an LP64 'unsigned long', and the shift is why. Shifting
   right by 60 needs a type at least 61 bits wide; where 'unsigned long' is 32
   bits - LLP64, which is Windows - the count reaches the width of the type and
   the shift is undefined behaviour rather than a different answer. The
   division above it is portable and gives 1431655765 on both data models.

   So the two compilers part company here and the standard permits both: cc1
   folds the shift the way the hardware performs it, x86 masking the count to
   five bits so that 60 becomes 28 and 0xFFFFFFFF >> 28 is 15, and cl folds it
   to 0 as though every bit had been shifted out. cl then emits code that
   answers 15 at run time, disagreeing with its own constant folder; cc1 gives
   15 both ways. Nothing here is a bug in either compiler, and the case is not
   evidence about anything on a target whose long is narrower than 61 bits. */
int main(void)
{
    switch (0) {
    case (int)((0UL - 1UL) / 3UL): return 9;      /* huge, truncates to 1431655765 */
    case (int)((0UL - 1UL) >> 60): return 9;      /* 15 unsigned, -1 signed */
    default: return 6;
    }
}
