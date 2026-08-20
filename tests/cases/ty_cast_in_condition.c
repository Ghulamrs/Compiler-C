// expect: 1
/* A cast in a condition is converted first and tested afterwards, which is the
   whole difference between it meaning something and it being decoration:
   (unsigned char)256 is zero, and a compiler that tested the operand instead
   of the conversion would call it true.
   The other three are the same conversions the ty_cast_* cases pin, asked
   where the answer decides a branch rather than a value. */
int main(void)
{
    int i = 456;                 /* 456 & 0xff is 200 */
    int neg = -1;
    int ok = 1;

    if ((unsigned char)i != 200) ok = 0;
    if ((unsigned char)neg <= 200) ok = 0;      /* 255 */
    if ((unsigned char)256) ok = 0;             /* zero, so false */
    if (!(signed char)200) ok = 0;              /* -56, so true */

    return ok;
}
