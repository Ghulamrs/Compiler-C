// expect: 0
// L'x' and L"..." - wide character and string literals, made of wchar_t.
//
// wchar_t is the one type in this corpus whose width was measured on each
// platform rather than reasoned about, because the three disagree and a
// program can see it: int on Linux and macOS, four bytes and signed, against
// unsigned short under the UCRT, two bytes and not. Every check below is
// written against sizeof(wchar_t) rather than against 4, so it asks whether
// the compiler agrees with its own target instead of with one of them.
#include <stddef.h>

static const wchar_t at_file_scope[] = L"abc";
static const wchar_t *pointed_at     = L"hi";

int main(void)
{
    wchar_t local[] = L"xy";
    const wchar_t *joined = L"hi" L"!";     /* phase 6 joins wide ones too */
    wchar_t ch = L'A';
    int i, n;

    /* A wide array is elements, not bytes: four characters here and a
       terminator, whatever a wchar_t happens to weigh. */
    if (sizeof at_file_scope != 4 * sizeof(wchar_t)) return 1;
    if (at_file_scope[0] != L'a' || at_file_scope[3] != 0) return 2;

    if (sizeof local != 3 * sizeof(wchar_t)) return 3;
    if (local[0] != L'x' || local[1] != L'y' || local[2] != 0) return 4;

    /* Read back through a pointer, which is where a wide literal put in a
       C-string section gets cut at its first zero byte and silently coalesced
       with something else. */
    if (pointed_at[0] != L'h' || pointed_at[1] != L'i' || pointed_at[2] != 0) return 5;

    n = 0;
    for (i = 0; joined[i] != 0; i++) n++;
    if (n != 3) return 6;

    /* The constant itself, including an escape and a zero. */
    if (ch != 65 || L'\n' != 10 || L'\0' != 0) return 7;

    /* sizeof(L"...") counts the terminator, like the narrow form. */
    if (sizeof(L"hi") != 3 * sizeof(wchar_t)) return 8;
    if (sizeof("hi") != 3) return 9;

    /* And the narrow forms are untouched by any of it. */
    {
        char narrow[] = "xy";
        const char *np = "hi";
        if (sizeof narrow != 3) return 10;
        if (np[0] != 'h' || np[2] != 0) return 11;
        if ('A' != 65) return 12;
    }

    return 0;
}
