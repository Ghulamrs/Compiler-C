// Type.h - what a value is, and who decides how big it is.
//
// See docs/TYPES.md. The one rule that shapes this file: a size is never a
// constant here. sizeof(long) is 8 on Linux and Apple and 4 on Windows, so a
// literal 8 in the front end would make the compiler silently wrong on one
// target while the tests on the other two stayed green. Every size, alignment
// and signedness question goes to the Target.
#pragma once

#include <string>
#include <vector>

enum class Kind {
    Void,
    Char, SChar, UChar,      // three distinct types, even though one shares a
    Short, UShort,           // representation with another - see docs/TYPES.md
    Int, UInt,
    Long, ULong,
    LongLong, ULongLong,
    Pointer, Array, Function // declared now, used from stage 2
};

class Target;

class Type {
public:
    explicit Type(Kind k) : kind_(k) {}

    Kind kind() const { return kind_; }

    bool isInteger() const {
        return kind_ >= Kind::Char && kind_ <= Kind::ULongLong;
    }
    bool isArithmetic() const { return isInteger(); }   // floating: stage 3
    bool isVoid() const { return kind_ == Kind::Void; }

    int size(const Target &t) const;
    int align(const Target &t) const;
    bool isSigned(const Target &t) const;

    // Conversion rank, for the usual arithmetic conversions. Signedness does
    // not affect rank: int and unsigned int rank equally, which is exactly why
    // the tie has to be broken by the signedness rules rather than by rank.
    int rank() const;

    const char *name() const;

private:
    Kind kind_;
};

// One Type object per distinct type, compared by pointer. Built once and never
// freed - a compiler is the one program that can legitimately never free.
class TypeTable {
public:
    TypeTable();
    const Type *get(Kind k) const;

    const Type *voidType() const   { return get(Kind::Void); }
    const Type *intType() const    { return get(Kind::Int); }
    const Type *charType() const   { return get(Kind::Char); }

private:
    std::vector<Type> types_;
};

// Everything that differs between Linux, Windows and Apple. Only the first
// exists today; the others are why this is a class and not a header of #defines.
class Target {
public:
    virtual ~Target() = default;

    virtual int sizeOf(Kind) const = 0;
    virtual int alignOf(Kind) const = 0;
    virtual bool plainCharIsSigned() const = 0;

    // What sizeof yields. unsigned long on LP64, unsigned long long on LLP64.
    virtual Kind sizeType() const = 0;

    virtual const char *name() const = 0;
};

// LP64: long and pointer are 64-bit, int is 32-bit.
class LinuxX86_64 final : public Target {
public:
    int sizeOf(Kind) const override;
    int alignOf(Kind) const override;
    bool plainCharIsSigned() const override { return true; }
    Kind sizeType() const override { return Kind::ULong; }
    const char *name() const override { return "x86_64-linux"; }
};
