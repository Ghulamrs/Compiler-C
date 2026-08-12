/* main.c - the driver.
 *
 *   cc1 <file.c> [-o out.s]
 *
 * Emits assembly only. Assembling and linking are gcc's job for now, which is
 * deliberate: it keeps the surface under test to the part actually being
 * written, and it means every generated program can be compared against what
 * gcc makes of the same source. That comparison is the test suite.
 */
#include "cc.h"

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "cannot open %s\n", path); exit(1); }

    if (fseek(fp, 0, SEEK_END) == -1) { fprintf(stderr, "seek failed\n"); exit(1); }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1) { fprintf(stderr, "seek failed\n"); exit(1); }

    char *buf = malloc((size_t)size + 2);
    if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
    size_t n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);

    /* A file not ending in a newline is legal C but awkward for the error
     * printer, which looks for one to bound the line. Supply it. */
    if (n == 0 || buf[n - 1] != '\n') buf[n++] = '\n';
    buf[n] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    const char *in = NULL;
    const char *out = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o")) {
            if (++i == argc) { fprintf(stderr, "-o needs an argument\n"); return 1; }
            out = argv[i];
        } else if (argv[i][0] == '-' && argv[i][1]) {
            fprintf(stderr, "unknown option %s\n", argv[i]);
            return 1;
        } else {
            in = argv[i];
        }
    }
    if (!in) { fprintf(stderr, "usage: cc1 <file.c> [-o out.s]\n"); return 1; }

    source_name = in;
    char *src = read_file(in);

    FILE *o = stdout;
    if (out) {
        o = fopen(out, "w");
        if (!o) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    }

    codegen(parse(tokenize(src)), o);

    if (o != stdout) fclose(o);
    return 0;
}
