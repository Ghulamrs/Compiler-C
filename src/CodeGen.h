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
#include <sstream>
#include <string>
#include <vector>

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;
};

class X86_64Linux final : public CodeGen {
public:
    X86_64Linux(std::ostream &sink, const Target &target)
        : sink_(sink), target_(target) {}

    void run(const Program &program) override;

    void visit(const Num &) override;
    void visit(const Var &) override;
    void visit(const Assign &) override;
    void visit(const Unary &) override;
    void visit(const Binary &) override;
    void visit(const Postfix &) override;
    void visit(const Call &) override;
    void visit(const Cast &) override;
    void visit(const StrLit &) override;
    void visit(const MemberAccess &) override;
    void visit(const ExprStmt &) override;
    void visit(const Return &) override;
    void visit(const Block &) override;
    void visit(const If &) override;
    void visit(const While &) override;
    void visit(const For &) override;
    void visit(const DoWhile &) override;
    void visit(const Switch &) override;
    void visit(const Case &) override;
    void visit(const Goto &) override;
    void visit(const Label &) override;
    void visit(const Conditional &) override;
    void visit(const Comma &) override;
    void visit(const Break &) override;
    void visit(const Continue &) override;

private:
    // Every function is emitted into its own buffer and the buffers are
    // concatenated in source order at the end. Two reasons, one of which is
    // useful today.
    //
    // Today: the order of the output no longer depends on the order of
    // emission, which is one fewer thing that can quietly change.
    //
    // Later: functions become independent of one another as producers of text.
    // Once there is an optimiser worth parallelising - and per-function passes
    // are where a real compiler spends its time - scheduling them across
    // threads becomes a change to this loop rather than a redesign of every
    // place that writes an instruction. See docs/PARALLELISM.md.
    std::ostringstream out_;          // the function being emitted
    std::vector<std::string> chunks_; // finished text, in source order
    std::ostream &sink_;              // where it all goes at the end

    const Target &target_;
    int depth_ = 0;
    int labels_ = 0;
    std::string returnLabel_;
    // Labels carry the function's name, so a per-function counter cannot
    // collide with another function's. A single shared counter would be the
    // one piece of state two threads could not both hold.
    std::string labelPrefix_;
    // Where break and continue go. A stack, because these nest, and continue is
    // not always the top of the loop: in a for it is the step.
    //
    // A switch is on this stack too, with an empty cont - it is something break
    // can leave and continue cannot. That is the whole difference between the
    // two statements, and holding it as an empty string means break is always
    // the top of the stack while continue looks down it for the first entry
    // that has one.
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

    void emit(const Function &fn);
    void finishChunk();
    // The only place a label's text is built. Definitions and jumps used to
    // spell it separately, which is exactly how half of them got renamed and
    // the other half did not.
    std::string label(const char *kind, int id) const;
    // A label the programmer named. Carries the same per-function prefix as
    // every other label, so two functions may both have a "done:" without the
    // assembler seeing one symbol defined twice.
    std::string userLabel(const std::string &name) const;
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
    // The three bit-field operations. A bit-field is the one place a value is
    // not a whole number of bytes at an address, so it needs its own path in
    // and out - and no path to an address at all, which is why genAddr refuses
    // one outright rather than quietly handing back the storage unit.
    //
    // unitAddr leaves the address of the storage unit in %rax. extract turns
    // that into the field's value, sign- or zero-extended by the member's own
    // type. insert writes %rax into the field at the address in %rdi, leaving
    // the stored value behind as the value of the assignment.
    void bitFieldUnitAddr(const MemberAccess &m);
    void bitFieldExtract(const MemberAccess &m);
    void bitFieldInsert(const MemberAccess &m);

    // Whole-object copy, from the address in %rax to the address in %rdi.
    // A struct assignment moves bytes; there is no register wide enough.
    void copyBlock(int size);

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
