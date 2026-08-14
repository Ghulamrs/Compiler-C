int f(void){return 3;} int (*get(void))(void){ return f; } int main(void){ return get()()-3; }
