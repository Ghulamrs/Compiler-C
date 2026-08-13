// expect: 5
int first(int *);
int count(char []);
int first(int *p) { return p[0]; }
int count(char s[]) { return s[0] == 'a'; }
int main(void)
{
    int a[2];
    a[0] = 4;
    return first(a) + count("a");
}
