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
    std::ostringstream out_;
    std::vector<std::string> chunks_;
    std::ostream &sink_;

    const Target &target_;
    int depth_ = 0;
    int labels_ = 0;
    std::string returnLabel_;
    std::string labelPrefix_;
    int sretSlot_ = 0;
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

    void emit(const Function &fn);
    void finishChunk();
    std::string label(const char *kind, int id) const;
    std::string userLabel(const std::string &name) const;
    void emitData(const Program &program);
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
};
