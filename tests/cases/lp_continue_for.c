// expect: 1
/* continue must run the step, or this never ends */
int main(void) { int odd = 0;
                 for (int i = 0; i < 10; i = i + 1) { if (i % 2 == 0) continue; odd = odd + 1; }
                 return odd == 5; }
