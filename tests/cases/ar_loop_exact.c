// expect: 1
/* Compares element by element rather than returning a sum. A sum can be
   wildly wrong and still exit with the right byte: with byte-scaled indexing
   the low byte of every a[i] is still i, so the total came out as 45 while
   every value in it was garbage. Equality is exact where the exit status is
   not. */
int main(void)
{
    int a[10];
    int i = 0;
    while (i < 10) { a[i] = i * 1000; i = i + 1; }
    return (a[0] == 0) * (a[1] == 1000) * (a[5] == 5000) * (a[9] == 9000);
}
