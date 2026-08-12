/* parse.c - tokens to a tree, by recursive descent.
 *
 * Hand-written rather than generated. bison 3.7.4 is on this box and would do
 * it, but a C grammar's hard parts - the declarator syntax, the typedef
 * ambiguity - are exactly the parts a generator does not solve for you, and a
 * hand-written parser is what lets an error say what it wanted.
 *
 * The grammar accepted today:
 *
 *   program = "int" "main" "(" "void" ")" "{" "return" expr ";" "}"
 *   expr    = mul ("+" mul | "-" mul)*
 *   mul     = unary ("*" unary | "/" unary)*
 *   unary   = ("+" | "-") unary | primary
 *   primary = num | "(" expr ")"
 *
 * Precedence is the shape of the call chain: expr calls mul, so mul binds
 * tighter. Left associativity is the loop rather than the recursion - folding
 * into lhs as it goes is what makes 8-3-2 parse as (8-3)-2 and not 8-(3-2).
 */
#include "cc.h"

static Token *tok; /* the cursor */

static int equal(Token *t, const char *s) {
    return (int)strlen(s) == t->len && !strncmp(t->loc, s, t->len);
}

static void expect(const char *s) {
    if (!equal(tok, s)) error_at(tok->loc, "expected '%s'", s);
    tok = tok->next;
}

static int consume(const char *s) {
    if (!equal(tok, s)) return 0;
    tok = tok->next;
    return 1;
}

static Node *new_node(NodeKind kind) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) error("out of memory");
    n->kind = kind;
    return n;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = new_node(kind);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

static Node *new_num(long v) {
    Node *n = new_node(ND_NUM);
    n->value = v;
    return n;
}

static Node *expr(void);

static Node *primary(void) {
    if (consume("(")) {
        Node *n = expr();
        expect(")");
        return n;
    }
    if (tok->kind == TK_NUM) {
        Node *n = new_num(tok->value);
        tok = tok->next;
        return n;
    }
    error_at(tok->loc, "expected an expression");
    return NULL; /* unreachable; error_at exits */
}

/* Unary plus is not a no-op to the parser even though it is one to the
 * machine: it has to be consumed, or "+3" fails to parse at primary. */
static Node *unary(void) {
    if (consume("+")) return unary();
    if (consume("-")) {
        Node *n = new_node(ND_NEG);
        n->lhs = unary();
        return n;
    }
    return primary();
}

static Node *mul(void) {
    Node *n = unary();
    for (;;) {
        if (consume("*"))      n = new_binary(ND_MUL, n, unary());
        else if (consume("/")) n = new_binary(ND_DIV, n, unary());
        else return n;
    }
}

static Node *expr(void) {
    Node *n = mul();
    for (;;) {
        if (consume("+"))      n = new_binary(ND_ADD, n, mul());
        else if (consume("-")) n = new_binary(ND_SUB, n, mul());
        else return n;
    }
}

Node *parse(Token *t) {
    tok = t;
    expect("int");
    if (!equal(tok, "main")) error_at(tok->loc, "only main() is supported yet");
    tok = tok->next;
    expect("(");
    consume("void"); /* "()" and "(void)" both accepted */
    expect(")");
    expect("{");
    expect("return");

    Node *ret = new_node(ND_RETURN);
    ret->lhs = expr();

    expect(";");
    expect("}");
    if (tok->kind != TK_EOF) error_at(tok->loc, "trailing text after '}'");
    return ret;
}
