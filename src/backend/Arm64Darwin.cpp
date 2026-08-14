#include "Arm64Darwin.h"

#include <cstdio>
#include <cstdlib>

int DarwinArm64Target::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                       return 1;
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 8;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Float:                                      return 4;
    case Kind::Double:                                     return 8;
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int DarwinArm64Target::alignOf(Kind k) const { return sizeOf(k); }

// AAPCS64 as Apple builds it. Eight integer registers, and the indirect return
// pointer travels in x8 - a register of its own, so unlike System V it does not
// push every other argument along by one.
static const char *const kArgRegs[] = { "x0", "x1", "x2", "x3",
                                        "x4", "x5", "x6", "x7" };
static const char *const kSseRegs[] = { "v0", "v1", "v2", "v3",
                                        "v4", "v5", "v6", "v7" };

static const Abi kAapcs64AppleAbi = {
    kArgRegs, 8,
    kSseRegs, 8,
    false,   // the two files are counted independently, as under System V
    0,       // no shadow space
    16,      // a struct over 16 bytes comes back through the pointer in x8
    true,    // an oversized aggregate is passed by reference
    false,   // no %al convention; and Apple puts variadic arguments on the stack
};

const Abi &Arm64DarwinBackend::abi() const { return kAapcs64AppleAbi; }

std::unique_ptr<CodeGen> Arm64DarwinBackend::codegen(std::ostream &) const {
    return nullptr;
}
