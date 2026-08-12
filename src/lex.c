/* lex.c - source text to a token list.
 *
 * One pass, no lookahead beyond the character in hand. Tokens keep a pointer
 * into the original source rather than a copy, so an error found three stages
 * later can still point at the column it came from.
 */
#include "cc.h"
#include <stdarg.h>

const char *source_start;
const char *source_name = "<stdin>";

static void verror_at(const char *loc, const char *fmt, va_list ap) {
    /* Find the line this offset sits on, so the caret lands under it. */
    const char *line = loc;
    while (line > source_start && line[-1] != '\n') line--;
    const char *end = loc;
    while (*end && *end != '\n') end++;

    int lineno = 1;
    for (const char *p = source_start; p < line; p++)
        if (*p == '\n') lineno++;

    int indent = fprintf(stderr, "%s:%d: ", source_name, lineno);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);
    fprintf(stderr, "%*s^ ", indent + (int)(loc - line), "");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void error_at(const char *loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
    va_end(ap);
    exit(1);
}

void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static Token *new_token(TokenKind kind, const char *start, int len) {
    Token *tok = calloc(1, sizeof(Token));
    if (!tok) error("out of memory");
    tok->kind = kind;
    tok->loc = start;
    tok->len = len;
    return tok;
}

/* The keyword set is checked after an identifier is scanned rather than
 * before, because "returned" and "integer" are identifiers and a prefix test
 * would take the front off both. */
static int is_keyword(const char *s, int len) {
    static const char *kw[] = { "int", "return", "void", NULL };
    for (int i = 0; kw[i]; i++)
        if ((int)strlen(kw[i]) == len && !strncmp(s, kw[i], len)) return 1;
    return 0;
}

static int ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int ident_cont(char c)  { return isalnum((unsigned char)c) || c == '_'; }

Token *tokenize(const char *src) {
    source_start = src;
    Token head = {0};
    Token *cur = &head;
    const char *p = src;

    while (*p) {
        if (isspace((unsigned char)*p)) { p++; continue; }

        /* A comment is whitespace that happens to be long. */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            const char *q = strstr(p + 2, "*/");
            if (!q) error_at(p, "unterminated comment");
            p = q + 2;
            continue;
        }

        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            long v = strtol(p, (char **)&p, 10);
            cur = cur->next = new_token(TK_NUM, start, (int)(p - start));
            cur->value = v;
            continue;
        }

        if (ident_start(*p)) {
            const char *start = p;
            while (ident_cont(*p)) p++;
            int len = (int)(p - start);
            cur = cur->next = new_token(
                is_keyword(start, len) ? TK_KEYWORD : TK_IDENT, start, len);
            continue;
        }

        if (strchr("+-*/(){};", *p)) {
            cur = cur->next = new_token(TK_PUNCT, p, 1);
            p++;
            continue;
        }

        error_at(p, "stray '%c' in program", *p);
    }

    cur->next = new_token(TK_EOF, p, 0);
    return head.next;
}
