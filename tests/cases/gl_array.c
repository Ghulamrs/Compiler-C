// expect: 1
int table[8];
int main(void) { table[3] = 99; return (table[3] == 99) * (sizeof table == 32); }
