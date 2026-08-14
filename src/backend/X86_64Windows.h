#pragma once

#include "Backend.h"

// x86-64 Windows. The instructions are the ones the Linux backend already
// emits; everything that differs is the ABI and the data model.
//
// LLP64 rather than LP64: long is 4 bytes here and 8 on Linux, which changes
// struct layouts, array strides and the result of every long computation. That
// is the difference this target exists to prove, since a size written as a
// literal anywhere in the front end is silently wrong on exactly one of them.
class WindowsX86_64Target final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULongLong; }
    const char *name() const override { return "x86_64-windows"; }
};

// Not written yet. What it will have to do differently from System V, all of it
// in the calling convention rather than in instruction selection:
//
//   - four integer argument registers, %rcx %rdx %r8 %r9, not six
//   - the integer and SSE files are positional, not independent lanes: the
//     third argument takes %r8 or %xmm2 by its position, so the rule the Linux
//     backend relies on - that one file can run out while the other has room -
//     is false here
//   - 32 bytes of shadow space allocated by the caller before every call
//   - a struct over 8 bytes is returned through a hidden pointer, not over 16
//   - a struct is passed by reference unless it is 1, 2, 4 or 8 bytes
class X86_64WindowsBackend final : public Backend {
public:
    const char *name() const override { return "x86_64-windows"; }
    const Target &target() const override { return target_; }
    int structReturnLimit() const override { return 8; }
    bool emits() const override { return false; }
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    WindowsX86_64Target target_;
};
