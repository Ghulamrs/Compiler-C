#include "X86_64Windows.h"

#include <cstdio>
#include <cstdlib>

int WindowsX86_64Target::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                       return 1;
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 4;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Float:                                      return 4;
    case Kind::Double:                                     return 8;
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int WindowsX86_64Target::alignOf(Kind k) const { return sizeOf(k); }

// Microsoft x64. Four integer registers rather than six, and the two files are
// positional - the third argument takes the third slot, %r8 or %xmm2, and
// spending one spends the other.
static const char *const kArgRegs[] = { "%rcx", "%rdx", "%r8", "%r9" };
static const char *const kSseRegs[] = { "%xmm0", "%xmm1", "%xmm2", "%xmm3" };

static const Abi kMsAbi = {
    kArgRegs, 4,
    kSseRegs, 4,
    true,    // positional: argument n takes slot n in whichever file
    32,      // shadow space the caller leaves for the callee to spill into
    8,       // a struct over 8 bytes comes back through a hidden pointer
    true,    // an oversized aggregate is passed as a pointer to the caller's copy
    false,   // no %al convention; a variadic callee reads its own shadow space
};

const Abi &X86_64WindowsBackend::abi() const { return kMsAbi; }

std::unique_ptr<CodeGen> X86_64WindowsBackend::codegen(std::ostream &) const {
    return nullptr;
}
