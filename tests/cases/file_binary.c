// expect: 0
// Binary-mode file I/O: whole structs out and back in, which is the demanding
// case because it asserts this compiler's struct layout against glibc's idea of
// the same bytes. gcc builds this too and must read back what it wrote in the
// same 24 bytes - int, four of padding, double, char[8].
#include <stdio.h>

struct Rec {
    int    id;
    double score;
    char   tag[8];
};

int main(void) {
    FILE *f;
    struct Rec out;
    struct Rec in;
    size_t n;
    long where;

    out.id = 7;
    out.score = 2.25;
    out.tag[0] = 'a';
    out.tag[1] = 'b';
    out.tag[2] = 0;

    printf("size: %lu\n", sizeof out);

    f = fopen("/tmp/cc1_case_bin.tmp", "wb");
    if (f == 0) return 1;
    n = fwrite(&out, sizeof out, 1, f);
    printf("wrote: %lu\n", n);
    fflush(f);
    where = ftell(f);
    printf("at: %ld\n", where);
    fclose(f);

    f = fopen("/tmp/cc1_case_bin.tmp", "rb");
    if (f == 0) return 2;

    in.id = 0;
    in.score = 0;
    in.tag[0] = 0;

    n = fread(&in, sizeof in, 1, f);
    printf("read: %lu\n", n);
    printf("back: %d %.2f %s\n", in.id, in.score, in.tag);

    where = ftell(f);
    printf("now: %ld\n", where);
    fseek(f, 0L, SEEK_SET);
    printf("seek: %ld\n", ftell(f));
    fseek(f, 0L, SEEK_END);
    printf("end: %ld\n", ftell(f));
    rewind(f);
    printf("rewound: %ld\n", ftell(f));
    printf("err: %d\n", ferror(f));
    fclose(f);

    remove("/tmp/cc1_case_bin.tmp");
    return 0;
}
