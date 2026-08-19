#pragma once

#include "Backend.h"

// x86-64 Windows. LLP64: long is 4 bytes here and 8 on Linux.
class WindowsX86_64Target final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULongLong; }
    // Two bytes and unsigned here, against four and signed on the other targets.
    Kind wcharType() const override { return Kind::UShort; }
    const char *name() const override { return "x86_64-windows"; }
};

// MASM is the default: it is the target's native assembler, so nothing in the
// path is borrowed from another toolchain.
void setWindowsAsmSyntax(bool gnu);

class X86_64WindowsBackend final : public Backend {
public:
    const char *name() const override { return "x86_64-windows"; }
    const Target &target() const override { return target_; }
    const Abi &abi() const override;
    bool emits() const override { return true; }
    const char *const *identityMacros() const override;
    bool emitsLineTable() const override;
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    WindowsX86_64Target target_;
};
