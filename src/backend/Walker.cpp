#include "Walker.h"

// The shapes here are the x86 backend's, chosen as canonical when the two
// walks were merged: an else-less 'if' branches straight to its end with no
// dead jump, and a loop's step label is called "step". The arm64 walk had
// the other choices; nothing observable turned on them, and the whole-corpus
// differential against clang is what proved the adoption changed no answer.

void Walker::visit(const ExprStmt &n) { n.expr().accept(*this); }

void Walker::visit(const Block &n) {
    for (const StmtPtr &s : n.body()) s->accept(*this);
}

void Walker::visit(const If &n) {
    int id = nextLabel();
    genTruth(n.cond());
    if (n.elseArm()) {
        branchIfZero(label("else", id));
        n.thenArm().accept(*this);
        jump(label("end", id));
        defineLabel(label("else", id));
        n.elseArm()->accept(*this);
    } else {
        branchIfZero(label("end", id));
        n.thenArm().accept(*this);
    }
    defineLabel(label("end", id));
}

void Walker::visit(const While &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("begin", id) });
    defineLabel(label("begin", id));
    genTruth(n.cond());
    branchIfZero(label("end", id));
    n.body().accept(*this);
    jump(label("begin", id));
    defineLabel(label("end", id));
    jumps_.pop_back();
}

void Walker::visit(const For &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    if (n.init()) n.init()->accept(*this);
    defineLabel(label("begin", id));
    if (n.cond()) {
        genTruth(*n.cond());
        branchIfZero(label("end", id));
    }
    n.body().accept(*this);
    defineLabel(label("step", id));
    if (n.step()) n.step()->accept(*this);
    jump(label("begin", id));
    defineLabel(label("end", id));

    jumps_.pop_back();
}

void Walker::visit(const DoWhile &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    defineLabel(label("begin", id));
    n.body().accept(*this);
    defineLabel(label("step", id));
    genTruth(n.cond());
    branchIfNotZero(label("begin", id));
    defineLabel(label("end", id));

    jumps_.pop_back();
}

void Walker::visit(const Switch &n) {
    int id = nextLabel();

    n.cond().accept(*this);
    for (const Case *c : n.cases())
        caseBranch(c->value(), label("case", c->id()));
    jump(n.defaultCase() ? label("default", n.defaultCase()->id())
                         : label("end", id));

    // A switch is a break target and not a continue target: 'continue' inside
    // one belongs to the loop around it.
    jumps_.push_back({ label("end", id), "" });
    n.body().accept(*this);
    jumps_.pop_back();
    defineLabel(label("end", id));
}

void Walker::visit(const Case &n) {
    defineLabel(label(n.isDefault() ? "default" : "case", n.id()));
    n.body().accept(*this);
}

void Walker::visit(const Goto &n) { jump(userLabel(n.label())); }

void Walker::visit(const Label &n) {
    defineLabel(userLabel(n.name()));
    n.body().accept(*this);
}

void Walker::visit(const Conditional &n) {
    int id = nextLabel();
    genTruth(n.cond());
    branchIfZero(label("else", id));
    n.thenArm().accept(*this);
    jump(label("end", id));
    defineLabel(label("else", id));
    n.elseArm().accept(*this);
    defineLabel(label("end", id));
}

void Walker::visit(const Comma &n) {
    n.left().accept(*this);
    n.right().accept(*this);
}

void Walker::visit(const Break &) { jump(jumps_.back().brk); }

void Walker::visit(const Continue &) {
    for (std::size_t i = jumps_.size(); i-- > 0;) {
        if (!jumps_[i].cont.empty()) {
            jump(jumps_[i].cont);
            return;
        }
    }
}
