#include "CodeGen.h"

#include <cstdio>
#include <cstdlib>
#include <ostream>

// A stack machine, deliberately. Register allocation is a later problem and a
// separable one; pushing every intermediate is always correct and makes the
// output readable against the tree that produced it. The cost is poor code,
// which is the right thing to be bad at first.

void X86_64Linux::push() {
    out_ << "  push %rax\n";
    depth_++;
}

void X86_64Linux::pop(const char *reg) {
    out_ << "  pop " << reg << "\n";
    depth_--;
}

void X86_64Linux::visit(const Num &n) {
    out_ << "  mov $" << n.value() << ", %rax\n";
}

void X86_64Linux::visit(const Unary &n) {
    n.operand().accept(*this);
    if (n.op() == '-') out_ << "  neg %rax\n";
}

void X86_64Linux::visit(const Binary &n) {
    // Right first, then left. Either order works for + and *, but - and / are
    // not commutative and popping the left side last is what puts it in %rax,
    // which is where the instruction wants it.
    n.rhs().accept(*this);
    push();
    n.lhs().accept(*this);
    pop("%rdi");
    // %rax = lhs, %rdi = rhs

    switch (n.op()) {
    case '+': out_ << "  add %rdi, %rax\n";  break;
    case '-': out_ << "  sub %rdi, %rax\n";  break;
    case '*': out_ << "  imul %rdi, %rax\n"; break;
    case '/':
        // cqo sign-extends %rax into %rdx:%rax, which is the dividend idiv
        // reads. Without it a negative numerator divides against garbage.
        out_ << "  cqo\n";
        out_ << "  idiv %rdi\n";
        break;
    default:
        std::fprintf(stderr, "codegen: unknown operator '%c'\n", n.op());
        std::exit(1);
    }
}

void X86_64Linux::visit(const Return &n) {
    n.value().accept(*this);
}

void X86_64Linux::run(const Node &program) {
    out_ << "  .globl main\n";
    out_ << "  .text\n";
    out_ << "main:\n";
    out_ << "  push %rbp\n";
    out_ << "  mov %rsp, %rbp\n";

    program.accept(*this);

    out_ << "  pop %rbp\n";
    out_ << "  ret\n";

    // A leaked push is a corrupted frame, and the symptom shows up in the
    // caller rather than here. Cheaper to assert than to debug.
    if (depth_ != 0) {
        std::fprintf(stderr, "codegen: stack depth %d at exit\n", depth_);
        std::exit(1);
    }
}
