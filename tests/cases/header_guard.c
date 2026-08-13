// expect: 0
// The same header twice, and <string.h> and <stdlib.h> both reaching
// <stddef.h>. Without the include guards the second size_t typedef would be a
// redefinition, and the program would be refused for a mistake it did not make.
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t n;

    n = strlen("guarded");
    printf("%lu %d\n", n, abs(-1));
    return 0;
}
