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
    Type(Kind k, const Type *pointee, long length)
        : kind_(k), pointee_(pointee), length_(length) {}

    Kind kind() const { return kind_; }

    // Pointer and Array only. An array's element type is its pointee, which is
    // also what it decays to a pointer to.
    const Type *pointee() const { return pointee_; }
    long length() const { return length_; }

    bool isPointer() const { return kind_ == Kind::Pointer; }
    bool isArray() const { return kind_ == Kind::Array; }
    // What can be added to, compared, and tested for truth.
    bool isScalar() const { return isInteger() || isPointer(); }

    bool isInteger() const {
        return kind_ >= Kind::Char && kind_ <= Kind::ULongLong;
    }
    bool isArithmetic() const { return isInteger(); }   // floating: stage 3
    bool isVoid() const { return kind_ == Kind::Void; }
    bool isComplete() const { return !isVoid() && !(isArray() && length_ < 0); }

    int size(const Target &t) const;
    int align(const Target &t) const;
    bool isSigned(const Target &t) const;

    // Conversion rank, for the usual arithmetic conversions. Signedness does
    // not affect rank: int and unsigned int rank equally, which is exactly why
    // the tie has to be broken by the signedness rules rather than by rank.
    int rank() const;

    const char *name() const;

    std::string describe() const;   // "char *", "int [16]", for diagnostics

private:
    Kind kind_;
    const Type *pointee_ = nullptr;
    long length_ = -1;
};

// One Type object per distinct type, compared by pointer. Built once and never
// freed - a compiler is the one program that can legitimately never free.
class TypeTable {
public:
    TypeTable();
    const Type *get(Kind k) const;

    // Interned: one object per distinct type, so pointer equality is type
    // equality even for "char **" reached by two different routes.
    const Type *pointerTo(const Type *t);
    const Type *arrayOf(const Type *t, long length);

    const Type *voidType() const   { return get(Kind::Void); }
    const Type *intType() const    { return get(Kind::Int); }
    const Type *charType() const   { return get(Kind::Char); }

private:
    std::vector<Type> types_;
    // A deque would do; this is a list of owned pointers because the vector
    // above must not reallocate under types that are referred to by address.
    std::vector<Type *> derived_;
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
