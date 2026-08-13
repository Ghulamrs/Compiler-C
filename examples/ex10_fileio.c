// FILE through an incomplete type, in both modes: text written and read back a
// line at a time, then a struct written as bytes and recovered.
#include <stdio.h>
#include "examples.h"

struct Record { int id; double score; char tag[8]; };

int ex_fileio(void) {
    FILE *f;
    char line[64];
    struct Record out;
    struct Record in;
    long where;
    int c;

    printf("[fileio]\n");

    f = fopen("/tmp/cc1_example.txt", "w");
    if (f == 0) { printf("  cannot write\n"); return 0; }
    fprintf(f, "%d %s %.2f\n", 42, "text", 1.5);
    fputs("second line\n", f);
    fputc('X', f);
    fclose(f);

    f = fopen("/tmp/cc1_example.txt", "r");
    if (f == 0) { printf("  cannot read\n"); return 0; }
    fgets(line, 64, f);
    printf("  line 1   : %s", line);
    fgets(line, 64, f);
    printf("  line 2   : %s", line);
    c = fgetc(f);
    printf("  char     : %c\n", c);
    c = fgetc(f);
    printf("  at eof   : %d %d\n", c == EOF, feof(f) != 0);
    fclose(f);

    out.id = 7;
    out.score = 2.25;
    out.tag[0] = 'a'; out.tag[1] = 'b'; out.tag[2] = 0;

    f = fopen("/tmp/cc1_example.bin", "wb");
    if (f == 0) { printf("  cannot write binary\n"); return 0; }
    printf("  wrote    : %lu record of %lu bytes\n",
           fwrite(&out, sizeof out, 1, f), sizeof out);
    fclose(f);

    f = fopen("/tmp/cc1_example.bin", "rb");
    if (f == 0) { printf("  cannot read binary\n"); return 0; }
    printf("  read     : %lu\n", fread(&in, sizeof in, 1, f));
    printf("  back     : %d %.2f %s\n", in.id, in.score, in.tag);
    where = ftell(f);
    fseek(f, 0L, SEEK_SET);
    printf("  seek     : %ld -> %ld\n", where, ftell(f));
    fclose(f);

    remove("/tmp/cc1_example.txt");
    remove("/tmp/cc1_example.bin");
    return 9;
}
