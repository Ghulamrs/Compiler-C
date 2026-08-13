/* The unit that owns the storage. It has never seen main.c. */
int shared = 41;
int table[4];
static int hidden = 99;

int bump(int by) { shared = shared + by; return shared; }
int hidden_probe(void) { return hidden; }
