#include "Walker.h"

#include "../Source.h"

// The shapes here are the x86 backend's, chosen as canonical when the two
// walks were merged: an else-less 'if' branches straight to its end with no
// dead jump, and a loop's step label is called "step". The arm64 walk had
// the other choices; nothing observable turned on them, and the whole-corpus
// differential against clang is what proved the adoption changed no answer.

// A statement's position becomes a file and a line the way a diagnostic's
// does - through Source - so the two can never name a line differently. The
// file number is one more than the index, DWARF counting its files from one.
void Walker::markLine(const Stmt &n) { markLine(n.pos()); }

void Walker::markLine(std::size_t pos) {
    if (lines_ == nullptr) return;
    // Offset zero means the parser built this statement rather than read it -
    // the stores an initialiser expands into are the case that matters. No C
    // statement can begin at the first byte of a file, a definition always
    // standing in front of it, so zero is free to mean "nowhere". Such a
    // statement is left under the line already in force, which is the line
    // its declaration was written on, rather than being sent to line 1.
    if (pos == 0) return;
    Source::Place at = lines_->locate(pos);
    emitLoc(at.file + 1, at.line, at.column);
}

void Walker::visit(const ExprStmt &n) { markLine(n); n.expr().accept(*this); }

void Walker::resetBlocks(const std::vector<int> &parents) {
    blocks_.clear();
    for (std::size_t i = 0; i < parents.size(); i++) {
        DwarfBlock b;
        b.parent = parents[i];
        blocks_.push_back(b);
    }
}

// Scope 0 is the function's own and is bounded by the subprogram; -1 is a
// block that opens no scope at all, which is what an initialiser's expansion
// is. Neither wants a label, and nor does anything without -g.
void Walker::openBlock(int scope) {
    if (lines_ == nullptr || scope <= 0) return;
    if (static_cast<std::size_t>(scope) >= blocks_.size()) return;
    blocks_[scope].begin = label("blk.b", scope);
    defineLabel(blocks_[scope].begin);
}

void Walker::closeBlock(int scope) {
    if (lines_ == nullptr || scope <= 0) return;
    if (static_cast<std::size_t>(scope) >= blocks_.size()) return;
    blocks_[scope].end = label("blk.e", scope);
    defineLabel(blocks_[scope].end);
}

void Walker::visit(const Block &n) {
    markLine(n);
    openBlock(n.scope());
    for (const StmtPtr &s : n.body()) s->accept(*this);
    closeBlock(n.scope());
}

void Walker::visit(const If &n) {
    markLine(n);
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
    markLine(n);
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
    markLine(n);
    // Around the whole loop and not just its body: what the init declares is
    // in scope for the condition and the step as well.
    openBlock(n.scope());
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    if (n.init()) n.init()->accept(*this);
    defineLabel(label("begin", id));
    if (n.cond()) {
        // Both of these are written on the 'for' line and both run after the
        // body, so without saying so again they would be attributed to
        // whatever the body's last statement was - and a debugger stepping
        // out of the body would land back on the line it just left.
        markLine(n);
        genTruth(*n.cond());
        branchIfZero(label("end", id));
    }
    n.body().accept(*this);
    defineLabel(label("step", id));
    if (n.step()) {
        markLine(n);
        n.step()->accept(*this);
    }
    jump(label("begin", id));
    defineLabel(label("end", id));
    closeBlock(n.scope());

    jumps_.pop_back();
}

void Walker::visit(const DoWhile &n) {
    markLine(n);
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
    markLine(n);
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
    markLine(n);
    defineLabel(label(n.isDefault() ? "default" : "case", n.id()));
    n.body().accept(*this);
}

void Walker::visit(const Goto &n) { markLine(n); jump(userLabel(n.label())); }

void Walker::visit(const Label &n) {
    markLine(n);
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

void Walker::visit(const Break &n) { markLine(n); jump(jumps_.back().brk); }

void Walker::visit(const Continue &n) {
    markLine(n);
    for (std::size_t i = jumps_.size(); i-- > 0;) {
        if (!jumps_[i].cont.empty()) {
            jump(jumps_[i].cont);
            return;
        }
    }
}
