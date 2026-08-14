typedef int F(void); int g(void){return 1;} int main(void){ F *p = g; return p()-1; }
