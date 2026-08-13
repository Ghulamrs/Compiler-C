// expect: 1
int main(void)
{
    char c = 'b';
    switch (c) {
    case 'a' + 1: return 1;
    case 'a' + 2: return 2;
    }
    return 0;
}
