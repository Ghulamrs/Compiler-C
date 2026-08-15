#pragma once

#include "Backend.h"

// x86-64 Windows. LLP64 rather than LP64: long is 4 bytes here and 8 on Linux,
// so a size written as a literal in the front end is silently wrong on exactly
// one of them - which is the difference this target exists to prove.
class WindowsX86_64Target final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULongLong; }
    const char *name() const override { return "x86_64-windows"; }
};

// Which spelling this backend writes. MASM is the default, because ml64 is the
// assembler this platform ships and link.exe is its linker; the GNU spelling
// stays reachable because tests/windows.sh assembles it with gcc on Linux, and
// because reading the two side by side is how the translation is checked.
void setWindowsAsmSyntax(bool gnu);

class X86_64WindowsBackend final : public Backend {
public:
    const char *name() const override { return "x86_64-windows"; }
    const Target &target() const override { return target_; }
    const Abi &abi() const override;
    bool emits() const override { return true; }
    const char *const *identityMacros() const override;
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    WindowsX86_64Target target_;
};
