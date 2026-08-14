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

std::unique_ptr<CodeGen> X86_64WindowsBackend::codegen(std::ostream &) const {
    return nullptr;
}
