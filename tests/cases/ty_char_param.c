// expect: 1
int takes(char c);
int takes(char c) { return c == -1; }
int main(void) { return takes(-1); }
