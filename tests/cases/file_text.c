// expect: 0
// Text-mode file I/O through the shipped <stdio.h>. gcc builds this against
// glibc's header, so FILE, EOF and every prototype below must agree with the
// real ones or the two binaries will not print the same bytes.
#include <stdio.h>

int main(void) {
    FILE *f;
    char line[64];
    int c;
    int n;

    f = fopen("/tmp/cc1_case_text.tmp", "w");
    if (f == 0) return 1;
    n = fprintf(f, "%d %s %.2f\n", 42, "text", 1.5);
    printf("fprintf wrote %d\n", n);
    fputs("second line\n", f);
    fputc('X', f);
    fputc('\n', f);
    fclose(f);

    f = fopen("/tmp/cc1_case_text.tmp", "r");
    if (f == 0) return 2;

    fgets(line, 64, f);
    printf("1: %s", line);
    fgets(line, 64, f);
    printf("2: %s", line);

    c = fgetc(f);
    printf("c: %c\n", c);
    c = fgetc(f);
    printf("nl: %d\n", c == '\n');

    c = fgetc(f);
    printf("eof: %d %d\n", c == EOF, feof(f) != 0);
    printf("err: %d\n", ferror(f));
    fclose(f);

    remove("/tmp/cc1_case_text.tmp");
    return 0;
}
