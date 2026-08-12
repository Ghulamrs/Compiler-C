/* cc.h - what the three stages hand each other.
 *
 * The compiler is written in C, not C++, for two reasons. Compiling it costs
 * about a fifth of what the equivalent C++ costs, which matters on a 419 MiB
 * box. And a C compiler written in C can eventually be fed to itself, which is
 * the only test that exercises every corner at once.
 */
#ifndef CC_H
#define CC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- tokens ---- */

typedef enum {
    TK_PUNCT,   /* + - * / ( ) { } ;            */
    TK_NUM,     /* an integer constant          */
    TK_IDENT,   /* an identifier                */
    TK_KEYWORD, /* int, return, void            */
    TK_EOF
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    long value;      /* TK_NUM */
    const char *loc; /* into the source, for error messages */
    int len;
};

Token *tokenize(const char *src);

/* Errors carry the offset into the source so the message can point at the
 * character rather than merely naming the file. Every stage reports this way. */
void error_at(const char *loc, const char *fmt, ...);
void error(const char *fmt, ...);

/* ---- the tree ---- */

typedef enum {
    ND_ADD, ND_SUB, ND_MUL, ND_DIV,
    ND_NEG,
    ND_NUM,
    ND_RETURN
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    long value; /* ND_NUM */
};

/* The whole program, for now: one function called main. */
Node *parse(Token *tok);

/* ---- code ---- */

void codegen(Node *node, FILE *out);

/* Set by main from argv, used by error_at to print the offending line. */
extern const char *source_start;
extern const char *source_name;

#endif /* CC_H */
