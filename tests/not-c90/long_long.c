/* 'long long' is C99, signed and unsigned alike. C90 stops at 'long', and a
   program using it here gets 64 bits from a type the standard has no name for. */
int main(void)
{
    long long a = 0;
    unsigned long long b = 0;
    return (int)(a + (long long)b);
}
