// expect: 6
/* Out of two loops at once, which is the thing break cannot do. */
int main(void)
{
    int i;
    int j;
    int r = 0;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            r = r + 1;
            if (r == 6) goto out;
        }
    }
out:
    return r;
}
