#pragma once

#include "Backend.h"

#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

// arm64 macOS - Apple silicon. LP64, like Linux x86-64, so the data model is
// the familiar one and long is 8 bytes.
//
// Two sizes still differ from Linux, and both are long double: Apple makes it
// plain double at 8 bytes, where AArch64 Linux makes it 128-bit quad and
// x86-64 Linux makes it 80-bit x87 padded to 16. That is the one type whose
// answer differs on all three of the targets here.
class DarwinArm64Target final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULong; }
    const char *name() const override { return "arm64-darwin"; }
};

// Not written yet, and the only one of the three that changes instruction
// selection rather than just the convention around it. What is new:
//
//   - a load-store architecture: no operand reads memory, so every access is
//     an explicit ldr or str and the stack machine gets longer, not different
//   - eight integer argument registers x0-x7, and the indirect return pointer
//     goes in x8 - a register of its own, so unlike System V it does not push
//     every other argument along by one
//   - Apple passes variadic arguments on the stack rather than in registers,
//     which is a deviation from AAPCS64 and not a detail: printf is the first
//     thing that notices
//   - Mach-O rather than ELF, and clang rather than gcc as the reference the
//     differential suite compares against
class Arm64DarwinBackend final : public Backend {
public:
    const char *name() const override { return "arm64-darwin"; }
    const Target &target() const override { return target_; }
    const Abi &abi() const override;
    bool emits() const override { return true; }
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    DarwinArm64Target target_;
};

// A stack machine again, for the same reason the x86-64 one is: always correct,
// poor code, and register allocation a later and separable problem. What is new
// is that this is load-store - no operand reads memory - so every access is an
// explicit address computation followed by ldr or str, and the sequences are
// longer rather than different.
//
// x0 carries an integer or pointer result, d0 a floating one. Locals live at
// negative offsets from x29. The address is always computed into a register
// rather than folded into the instruction, because AArch64 offset encodings are
// limited and a frame can outgrow them: uniform and slow beats fast and wrong
// past the 256th byte.
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
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

    int nextLabel() { return labels_++; }
    std::string label(const char *kind, int id) const;
    std::string userLabel(const std::string &name) const;

    void unsupported(const char *what);

    void push();
    void pop(const char *reg);
    void emitData(const Program &program);
    void emitFunction(const Function &fn);

    void genAddr(const Expr &e);
    void load(const Type *t);
    void storeThrough(const Type *t, const char *addrReg);
    void genTruth(const Expr &e);
    void movImm(const char *reg, long value);
};
