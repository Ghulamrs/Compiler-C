// expect: 14
int g = 2 + 3 * 4;
int main(void)
{
    switch (g) {
    case 2 + 3 * 4: return 14;
    default: return 0;
    }
}
