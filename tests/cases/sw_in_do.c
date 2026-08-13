// expect: 6
int main(void)
{
    int i = 0;
    int r = 0;
    do {
        switch (i) {
        case 0:
        case 1:
            r = r + 1;
            break;
        case 2:
            r = r + 4;
            break;
        }
        i = i + 1;
    } while (i < 3);
    return r;
}
