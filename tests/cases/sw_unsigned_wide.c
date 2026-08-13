// expect: 42
/* A case value past the 32-bit immediate cmp takes. It has to be materialised
   or the comparison is against a different number than the one written. */
int main(void)
{
    unsigned int u = 4294967295u;
    switch (u) {
    case 0: return 1;
    case 4294967295u: return 42;
    }
    return 0;
}
