// CodeGen.h - the tree to assembly.
//
// One class per target, all of them visitors. Today there is one:
// X86_64Linux, emitting System V assembly in GNU as syntax. The two to come
// are X86_64Windows and Arm64Apple, and they are separate classes rather than
// flags on this one because they differ in the convention, not just the
// spelling: Windows passes integers in RCX/RDX/R8/R9, makes the caller reserve
// 32 bytes of shadow space, treats RSI and RDI as callee-saved, and has no red
// zone. Apple's arm64 puts variadic arguments on the stack where Linux arm64
// puts them in registers.
//
// <iosfwd> rather than <ostream>: every translation unit including this header
// pays for what it pulls in, and a class-heavy C++ unit already costs about
// 180 MB to compile on this machine.
#pragma once

#include "Ast.h"

#include <iosfwd>

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Node &program) = 0;
};

class X86_64Linux final : public CodeGen {
public:
    explicit X86_64Linux(std::ostream &out) : out_(out) {}

    void run(const Node &program) override;

    void visit(const Num &) override;
    void visit(const Unary &) override;
    void visit(const Binary &) override;
    void visit(const Return &) override;

private:
    std::ostream &out_;
    int depth_ = 0;   // pushes not yet popped; checked at the end

    void push();
    void pop(const char *reg);
};
