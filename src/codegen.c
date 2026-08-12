/* codegen.c - tree to x86-64 assembly, System V ABI, GNU as syntax.
 *
 * A stack machine, deliberately. Register allocation is a later problem and a
 * separable one; pushing every intermediate is always correct and makes the
 * output easy to read against the tree that produced it. The cost is dreadful
 * code, which is the right thing to be bad at first.
 *
 * The right operand is generated before the left. Both end up on the stack, so
 * either order works for + and *, but - and / are not commutative and popping
 * lhs last is what puts it in %rax where the instruction wants it.
 */
#include "cc.h"

static int depth; /* pushes not yet popped - checked at the end */

static void push(FILE *o) {
    fprintf(o, "  push %%rax\n");
    depth++;
}

static void pop(FILE *o, const char *reg) {
    fprintf(o, "  pop %s\n", reg);
    depth--;
}

static void gen_expr(Node *n, FILE *o) {
    switch (n->kind) {
    case ND_NUM:
        fprintf(o, "  mov $%ld, %%rax\n", n->value);
        return;
    case ND_NEG:
        gen_expr(n->lhs, o);
        fprintf(o, "  neg %%rax\n");
        return;
    default:
        break;
    }

    gen_expr(n->rhs, o);
    push(o);
    gen_expr(n->lhs, o);
    pop(o, "%rdi");
    /* %rax = lhs, %rdi = rhs */

    switch (n->kind) {
    case ND_ADD:
        fprintf(o, "  add %%rdi, %%rax\n");
        return;
    case ND_SUB:
        fprintf(o, "  sub %%rdi, %%rax\n");
        return;
    case ND_MUL:
        fprintf(o, "  imul %%rdi, %%rax\n");
        return;
    case ND_DIV:
        /* cqo sign-extends %rax into %rdx:%rax, which is the dividend idiv
         * reads. Without it a negative numerator divides against garbage. */
        fprintf(o, "  cqo\n");
        fprintf(o, "  idiv %%rdi\n");
        return;
    default:
        error("codegen: unexpected node");
    }
}

void codegen(Node *node, FILE *o) {
    fprintf(o, "  .globl main\n");
    fprintf(o, "  .text\n");
    fprintf(o, "main:\n");
    fprintf(o, "  push %%rbp\n");
    fprintf(o, "  mov %%rsp, %%rbp\n");

    if (node->kind != ND_RETURN) error("codegen: expected a return");
    gen_expr(node->lhs, o);

    fprintf(o, "  pop %%rbp\n");
    fprintf(o, "  ret\n");

    /* A leaked push is a corrupted frame, and the symptom is a crash in the
     * caller rather than here. Cheaper to assert than to debug. */
    if (depth != 0) error("codegen: stack depth %d at exit", depth);
}
