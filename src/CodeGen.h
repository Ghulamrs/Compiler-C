// CodeGen.h - the typed tree to assembly.
//
// One class per target, all of them visitors. Today there is one:
// X86_64Linux, emitting System V assembly in GNU as syntax. Windows and Apple
// differ in the convention, not the spelling - RCX/RDX/R8/R9 with 32 bytes of
// shadow space, and variadic arguments on the stack rather than in registers.
//
// Two invariants hold everywhere in this file.
//
// An expression leaves its value in %rax if its type is integer or a pointer,
// and in %xmm0 if it is floating. Which one is never guessed - the type says.
// An integer value in %rax is always held sign- or zero-extended to 64 bits
// according to its own type, which is what lets a char and a long share the
// register without every operation asking what it is holding.
//
// Every conversion is already a Cast node in the tree. The parser put them
// there. Nothing here knows a promotion rule; it only knows how to widen and
// narrow what it is handed.
#pragma once

#include "Ast.h"
#include "Type.h"

#include <iosfwd>
#include <string>

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;
};

class X86_64Linux final : public CodeGen {
public:
    X86_64Linux(std::ostream &out, const Target &target)
        : out_(out), target_(target) {}

    void run(const Program &program) override;

    void visit(const Num &) override;
    void visit(const Var &) override;
    void visit(const Assign &) override;
    void visit(const Unary &) override;
    void visit(const Binary &) override;
    void visit(const Call &) override;
    void visit(const Cast &) override;
    void visit(const StrLit &) override;
    void visit(const ExprStmt &) override;
    void visit(const Return &) override;
    void visit(const Block &) override;
    void visit(const If &) override;
    void visit(const While &) override;

private:
    std::ostream &out_;
    const Target &target_;
    int depth_ = 0;
    int labels_ = 0;
    std::string returnLabel_;

    void emit(const Function &fn);
    void emitData(const Program &program);
    void push();
    void pop(const char *reg);
    // The floating stack. Kept separate because %xmm registers cannot be
    // pushed, so the slot has to be made by hand.
    void pushF();
    void popF(const char *reg);
    int nextLabel() { return labels_++; }

    // The address of an lvalue, left in %rax. Assignment, '&' and every read
    // of a named object go through this - which is what let "*p = x" and
    // "a[i] = x" exist at all, since neither names a slot.
    void genAddr(const Expr &e);

    // Width-correct memory access, through an address rather than an offset.
    // load reads from %rax and leaves the value there, extended by signedness;
    // store writes %rax through the address in %rdi.
    void load(const Type *t);
    void store(const Type *t);
    // Straight to a frame slot, touching no address register. The prologue
    // needs this: using %rdi to hold the destination destroyed the incoming
    // %rdi, which is the next argument.
    void storeAt(const Type *t, int offset);

    // Put %rax back into canonical form for t after an operation that may have
    // left the high bits wrong.
    void canonicalise(const Type *t);
    void genFloatBinary(const Binary &n);
    void genConversion(const Type *from, const Type *to);
    // Evaluate e and leave 0 or 1 in %rax, whichever register file it used.
    // Conditions have to work the same way for a double as for an int.
    void genTruth(const Expr &e);

    // "%rax" or "%eax" and "%rdi" or "%edi", by the width the operation runs at.
    const char *acc(const Type *t) const;
    const char *rhs(const Type *t) const;
};
