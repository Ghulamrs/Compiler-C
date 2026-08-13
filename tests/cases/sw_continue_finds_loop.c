// expect: 8
/* continue looks past the switch to the loop, which is the one thing that
   distinguishes it from break. */
int main(void)
{
    int i;
    int r = 0;
    for (i = 0; i < 5; ++i) {
        switch (i) {
        case 2: continue;
        default: break;
        }
        r = r + i;
    }
    return r;
}
