#pragma once

#include "Backend.h"

#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

// x86-64 System V, GNU as syntax. LP64: long is 8 bytes.
class LinuxX86_64Target final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULong; }
    Kind wcharType() const override { return Kind::Int; }
    const char *name() const override { return "x86_64-linux"; }
};

class X86_64LinuxBackend final : public Backend {
public:
    const char *name() const override { return "x86_64-linux"; }
    const Target &target() const override { return target_; }
    const Abi &abi() const override;
    bool emits() const override { return true; }
    const char *const *identityMacros() const override;
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    LinuxX86_64Target target_;
};

class X86_64Linux : public CodeGen {
public:
    X86_64Linux(std::ostream &sink, const Target &target, const Abi &abi)
        : sink_(sink), target_(target), abi_(abi) {}

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
    void visit(const VaStart &) override;
    void visit(const VaArg &) override;
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
    std::ostringstream out_;
    std::vector<std::string> chunks_;
    std::ostream &sink_;

    const Target &target_;
    const Abi &abi_;
    int depth_ = 0;
    int labels_ = 0;
    std::string returnLabel_;
    std::string labelPrefix_;
    int sretSlot_ = 0;
    int regSave_ = 0;
    int varGp_ = 0, varFp_ = 48, varOverflow_ = 16;
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

    void emit(const Function &fn);
    void finishChunk();
    std::string label(const char *kind, int id) const;
    std::string userLabel(const std::string &name) const;
    void emitData(const Program &program);
    void emitGlobal(const Global &g, Segment seg);
    void push();
    void pop(const char *reg);
    void pushF();
    void popF(const char *reg);
    int nextLabel() { return labels_++; }

    void genAddr(const Expr &e);

    void load(const Type *t);
    void store(const Type *t);
    void storeAt(const Type *t, int offset);
    void bitFieldUnitAddr(const MemberAccess &m);
    void bitFieldExtract(const MemberAccess &m);
    void bitFieldInsert(const MemberAccess &m);

    void copyBlock(int size);

    void canonicalise(const Type *t);
    void genFloatBinary(const Binary &n);
    void genConversion(const Type *from, const Type *to);
    void genTruth(const Expr &e);

    const char *acc(const Type *t) const;
    const char *rhs(const Type *t) const;

    void unsupported(const char *what);
    // Microsoft x64 aggregate in %rax: either the bytes themselves, or the
    // address of the caller's copy of them.
    void msAggregateToRax(const Type *t, int slot);
    void msCopyToSlot(const Type *t, int slot, const char *from);
    int takeSlot(bool sse, int &ints, int &sses) const;
};

// System V's eightbyte classification: one bool per eightbyte, true when
// everything overlapping it is float or double. No other ABI has it, which is
// why it is here rather than beside a type model meant to be platform-neutral.
std::vector<bool> classifyEightbytes(const Type *t, const Target &target);
