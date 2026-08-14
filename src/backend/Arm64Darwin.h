#pragma once

#include "Backend.h"

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
    int structReturnLimit() const override { return 16; }
    bool emits() const override { return false; }
    std::unique_ptr<CodeGen> codegen(std::ostream &sink) const override;
private:
    DarwinArm64Target target_;
};
