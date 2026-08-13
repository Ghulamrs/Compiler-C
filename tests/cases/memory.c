// expect: 0
// The memory half of the shipped library: allocation from <stdlib.h> and the
// byte functions from <string.h>. Nothing here is implemented by this compiler
// - every one of them is glibc's, reached through an ordinary prototype - so
// what the case checks is that the prototypes agree with the real ones. gcc
// builds the same file against the real headers, and the two must print the
// same bytes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Node { int value; struct Node *next; };

int main(void) {
    int *a;
    char *s;
    char buf[16];
    struct Node *head, *n;
    int i, sum;

    a = (int *)malloc(10 * sizeof(int));
    if (a == NULL) return 1;
    for (i = 0; i < 10; i = i + 1) a[i] = i * i;
    printf("malloc  : %d %d %d\n", a[0], a[5], a[9]);

    // realloc keeps what was there and gives back more room.
    a = (int *)realloc(a, 20 * sizeof(int));
    if (a == NULL) return 2;
    for (i = 10; i < 20; i = i + 1) a[i] = i;
    printf("realloc : kept %d %d, added %d\n", a[5], a[9], a[19]);
    free(a);

    a = (int *)calloc(8, sizeof(int));
    if (a == NULL) return 3;
    sum = 0;
    for (i = 0; i < 8; i = i + 1) sum = sum + a[i];
    printf("calloc  : sum of zeroes %d\n", sum);
    free(a);

    // Overlapping, which is memmove's whole reason to exist.
    strcpy(buf, "abcdefgh");
    memmove(buf + 2, buf, 6);
    buf[8] = 0;
    printf("memmove : %s\n", buf);

    strcpy(buf, "hello world");
    printf("memchr  : %d\n", (int)((char *)memchr(buf, 'w', 11) - buf));
    printf("strchr  : %d %d\n", (int)(strchr(buf, 'o') - buf),
           strchr(buf, 'z') == NULL);
    printf("strrchr : %d\n", (int)(strrchr(buf, 'o') - buf));
    printf("strstr  : %d\n", (int)(strstr(buf, "wor") - buf));
    printf("strspn  : %lu %lu\n", strspn(buf, "hel"), strcspn(buf, "w"));
    printf("strpbrk : %d\n", (int)(strpbrk(buf, "wr") - buf));

    s = (char *)malloc(32);
    strcpy(s, "allocated");
    printf("string  : %s (%lu)\n", s, strlen(s));
    free(s);

    // A list, which is what an allocator is usually for.
    head = NULL;
    for (i = 3; i > 0; i = i - 1) {
        n = (struct Node *)malloc(sizeof(struct Node));
        n->value = i * 100;
        n->next = head;
        head = n;
    }
    printf("list    :");
    n = head;
    while (n != NULL) { printf(" %d", n->value); n = n->next; }
    printf("\n");
    while (head != NULL) { n = head->next; free(head); head = n; }

    free(NULL);                  /* defined, and does nothing */
    printf("free(0) : survived\n");
    printf("sizes   : %lu %lu\n", sizeof(struct Node), sizeof(size_t));
    return 0;
}
