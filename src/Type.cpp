#include "Type.h"

#include <cstdio>
#include <cstdlib>

TypeTable::TypeTable() {
    // Order matters: get() indexes by Kind.
    for (int k = static_cast<int>(Kind::Void);
         k <= static_cast<int>(Kind::Function); k++)
        types_.push_back(Type(static_cast<Kind>(k)));
}

const Type *TypeTable::get(Kind k) const {
    return &types_[static_cast<std::size_t>(k)];
}

int Type::size(const Target &t) const { return t.sizeOf(kind_); }
int Type::align(const Target &t) const { return t.alignOf(kind_); }

bool Type::isSigned(const Target &t) const {
    switch (kind_) {
    case Kind::Char:      return t.plainCharIsSigned();  // the target's call
    case Kind::SChar:
    case Kind::Short:
    case Kind::Int:
    case Kind::Long:
    case Kind::LongLong:  return true;
    default:              return false;
    }
}

// Signedness deliberately does not affect rank. int and unsigned int rank
// equally, which is what forces the usual arithmetic conversions to break the
// tie on signedness instead - and is why -1 < 1u is false.
int Type::rank() const {
    switch (kind_) {
    case Kind::Char: case Kind::SChar: case Kind::UChar:       return 1;
    case Kind::Short: case Kind::UShort:                       return 2;
    case Kind::Int: case Kind::UInt:                           return 3;
    case Kind::Long: case Kind::ULong:                         return 4;
    case Kind::LongLong: case Kind::ULongLong:                 return 5;
    default:                                                   return 0;
    }
}

const char *Type::name() const {
    switch (kind_) {
    case Kind::Void:      return "void";
    case Kind::Char:      return "char";
    case Kind::SChar:     return "signed char";
    case Kind::UChar:     return "unsigned char";
    case Kind::Short:     return "short";
    case Kind::UShort:    return "unsigned short";
    case Kind::Int:       return "int";
    case Kind::UInt:      return "unsigned int";
    case Kind::Long:      return "long";
    case Kind::ULong:     return "unsigned long";
    case Kind::LongLong:  return "long long";
    case Kind::ULongLong: return "unsigned long long";
    case Kind::Pointer:   return "pointer";
    case Kind::Array:     return "array";
    case Kind::Function:  return "function";
    }
    return "?";
}

// LP64, measured against gcc on the development box rather than recalled:
// char 1, short 2, int 4, long 8, long long 8, pointer 8.
int LinuxX86_64::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                   return 1;  // gcc's extension
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 8;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

// Equal to size for every scalar on this target. Arrays will take the
// alignment of their element rather than of their total - char[16] is sixteen
// bytes aligned to one, measured.
int LinuxX86_64::alignOf(Kind k) const { return sizeOf(k); }
