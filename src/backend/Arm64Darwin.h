#pragma once

#include "Backend.h"

#include <iosfwd>
#include <sstream>
#include <set>
#include <string>
#include <vector>

// arm64 macOS - Apple silicon. LP64, as Linux x86-64 is; the sizes that differ
// are in docs/TYPES.md.
class DarwinArm64Target final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULong; }
    const char *name() const override { return "arm64-darwin"; }
};

class Arm64DarwinBackend final : public Backend {
public:
    const char *name() const override { return "arm64-darwin"; }
    const Target &target() const override { return target_; }
    const Abi &abi() const override;
    bool emits() const override { return true; }
    const char *const *identityMacros() const override;
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    DarwinArm64Target target_;
};

// x0 carries an integer or pointer result, d0 a floating one; locals live at
// negative offsets from x29.
class Arm64Darwin final : public CodeGen {
public:
    Arm64Darwin(std::ostream &sink, const Target &target, const Abi &abi)
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
    std::ostream &sink_;
    const Target &target_;
    const Abi &abi_;

    int labels_ = 0;
    std::string returnLabel_;
    std::string labelPrefix_;
    std::string functionName_;
    // What this file defines. Anything else named in it is imported, and on
    // Darwin an imported symbol is reached through the GOT rather than by
    // page-addressing a place that is not in this image.
    std::set<std::string> definedHere_;
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

    int nextLabel() { return labels_++; }
    std::string label(const char *kind, int id) const;
    std::string userLabel(const std::string &name) const;

    void unsupported(const char *what);

    void push();
    void pop(const char *reg);
    void pushD();
    void popD(const char *reg);
    void emitData(const Program &program);
    void emitFunction(const Function &fn);

    // Where one aggregate travels under AAPCS64. An HFA of one to four
    // same-typed floats goes in that many vector registers whatever its size;
    // anything else of 16 bytes or less goes in one or two integer registers;
    // anything larger is copied by the caller and passed as a pointer.
    struct AggPlan {
        int hfa = 0;                 // vector registers, 0 when not an HFA
        Kind elem = Kind::Double;
        int words = 0;               // integer registers, when not an HFA
        bool byRef = false;          // one integer register, holding a pointer
    };
    AggPlan planFor(const Type *t) const;
    void storeWord(const char *xreg, const char *base, int k, int size);
    int sretSlot_ = 0;

    void genAddr(const Expr &e);
    void addOffset(int bytes);
    void copyBlock(int size, const char *from, const char *to);
    void bitFieldUnitAddr(const MemberAccess &m);
    void bitFieldExtract(const MemberAccess &m);
    void bitFieldInsert(const MemberAccess &m);
    void load(const Type *t);
    void storeThrough(const Type *t, const char *addrReg);
    void genTruth(const Expr &e);
    void movImm(const char *reg, long value);
    void loadFpConst(const std::string &reg, const Type *t, double v);
    void genConversion(const Type *from, const Type *to);
    void narrowInt(const Type *to);
};
