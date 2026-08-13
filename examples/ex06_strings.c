// The string functions from the shipped <string.h>, over char arrays that
// decay to pointers at the call.
#include <stdio.h>
#include <string.h>
#include "examples.h"

static void reverse(char *s) {
    int n = (int)strlen(s);
    int i = 0;
    int j = n - 1;

    while (i < j) {
        char t = s[i];
        s[i] = s[j];
        s[j] = t;
        i = i + 1;
        j = j - 1;
    }
}

int ex_strings(void) {
    char buf[32];
    char copy[32];
    char zeros[8];

    printf("[strings]\n");
    strcpy(buf, "compiler");
    printf("  strlen   : %lu\n", strlen(buf));

    strcat(buf, "-c");
    printf("  strcat   : %s\n", buf);

    printf("  strcmp   : %d %d %d\n", strcmp("abc", "abc"),
           strcmp("abc", "abd") < 0, strcmp("abd", "abc") > 0);
    printf("  strncmp  : %d %d\n", strncmp("abc", "abd", 2),
           strncmp("abc", "abd", 3) < 0);

    memset(zeros, 0, 8);
    printf("  memset   : %d %d\n", zeros[0], zeros[7]);

    memcpy(copy, buf, strlen(buf) + 1);
    printf("  memcpy   : %s (memcmp %d)\n", copy, memcmp(copy, buf, 5));

    reverse(copy);
    printf("  reversed : %s\n", copy);
    return 8;
}
