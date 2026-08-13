// expect: 3
/* The cast is where the width of the answer is decided. */
int main(void)
{
    switch (2) {
    case (char)257: return 1;
    case (char)258: return 3;
    default: return 9;
    }
}
