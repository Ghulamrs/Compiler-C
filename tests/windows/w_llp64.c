// expect: 48
// no-reference: long is 4 bytes on this target and 8 to the gcc on this box
// LLP64, which is the difference this target exists to prove. long is 4 bytes
// and long long is 8, where Linux makes both 8 - so sizeof(long) * 10 plus
// sizeof(long long) is 48 here and 88 there. Asking gcc would not be a second
// opinion about the same question; it would be an answer to a different one.
int main(void)
{
    return sizeof(long) * 10 + sizeof(long long);
}
