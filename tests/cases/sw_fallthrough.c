// expect: 60
/* A case runs into the next one unless something stops it. */
int main(void)
{
    int r = 0;
    switch (2) {
    case 1: r = r + 10;
    case 2: r = r + 20;
    case 3: r = r + 40;
        break;
    case 4: r = r + 80;
    }
    return r;
}
