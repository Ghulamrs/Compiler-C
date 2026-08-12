// expect: 1
int main(void)
{
    char string[16];
    string[0] = 72;
    string[1] = 105;
    string[2] = 0;
    return (sizeof string == 16) * (string[0] == 72) * (string[1] == 105);
}
