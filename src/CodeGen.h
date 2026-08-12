// CodeGen.h - the tree to assembly.
//
// One class per target, all of them visitors. Today there is one:
// X86_64Linux, emitting System V assembly in GNU as syntax. The two to come
// are X86_64Windows and Arm64Apple, and they are separate classes rather than
// flags on this one because they differ in the convention, not the spelling.
// Now that calls exist, that difference is concrete rather than anticipated:
// this file puts the first six integer arguments in RDI, RSI, RDX, RCX, R8, R9
// and keeps %rsp 16-byte aligned across a call. Windows uses RCX, RDX, R8, R9,
// makes the caller reserve 32 bytes of shadow space, treats RSI and RDI as
// callee-saved and has no red zone. Apple's arm64 puts variadic arguments on
// the stack where Linux arm64 puts them in registers.
//
// <iosfwd> rather than <ostream>: a class-heavy translation unit costs about
// 180 MB to compile on this 419 MiB box, and every include in a shared header
// is paid for by every unit that reads it.
#pragma once

#include "Ast.h"

#include <iosfwd>
#include <string>

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;
};

class X86_64Linux final : public CodeGen {
public:
    explicit X86_64Linux(std::ostream &out) : out_(out) {}

    void run(const Program &program) override;

    void visit(const Num &) override;
    void visit(const Var &) override;
    void visit(const Assign &) override;
    void visit(const Unary &) override;
    void visit(const Binary &) override;
    void visit(const Call &) override;
    void visit(const ExprStmt &) override;
    void visit(const Return &) override;
    void visit(const Block &) override;
    void visit(const If &) override;
    void visit(const While &) override;

private:
    std::ostream &out_;
    int depth_ = 0;              // pushes not yet popped; also the alignment parity
    int labels_ = 0;             // every if and while takes a fresh number
    std::string returnLabel_;    // where a return statement jumps, per function

    void emit(const Function &fn);
    void push();
    void pop(const char *reg);
    int nextLabel() { return labels_++; }
};
