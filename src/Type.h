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
    Float, Double,           // long double is deferred; see docs/TYPES.md
    Struct, Union,           // enum is an alias for int; see docs/TYPES.md
    Pointer, Array, Function
};

class Target;

class Type;

// A member of a struct or a union. Unions give every member offset zero, which
// is the only difference between the two once layout is done.
// A member of a struct or a union. Unions give every member offset zero, which
// is the only difference between the two once layout is done.
//
// A bit-field carries two more numbers and breaks one rule. offset is then the
// byte offset of the *storage unit* it lives in rather than of the member, and
// bitOffset says where inside that unit it starts - counted from the least
// significant bit, which is what little-endian means here. width is 0 for
// everything that is not a bit-field, and that is the test used everywhere.
//
// The rule it breaks: a bit-field has no address. C forbids taking one, so a
// member is no longer something genAddr can always be asked for.
struct Member {
    std::string name;
    const Type *type;
    int offset;
    int width = 0;        // bits; 0 means "not a bit-field"
    int bitOffset = 0;    // from the low bit of the storage unit at offset

    bool isBitField() const { return width != 0; }
};

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
    bool isScalar() const { return isArithmetic() || isPointer(); }

    bool isInteger() const {
        return kind_ >= Kind::Char && kind_ <= Kind::ULongLong;
    }
    bool isFloating() const { return kind_ == Kind::Float || kind_ == Kind::Double; }
    bool isArithmetic() const { return isInteger() || isFloating(); }
    bool isVoid() const { return kind_ == Kind::Void; }
    bool isStructOrUnion() const { return kind_ == Kind::Struct || kind_ == Kind::Union; }
    bool isComplete() const {
        if (isVoid()) return false;
        if (isArray() && length_ < 0) return false;
        if (isStructOrUnion()) return complete_;   // "struct Node;" is not
        return true;
    }

    int size(const Target &t) const;
    int align(const Target &t) const;
    bool isSigned(const Target &t) const;

    // Conversion rank, for the usual arithmetic conversions. Signedness does
    // not affect rank: int and unsigned int rank equally, which is exactly why
    // the tie has to be broken by the signedness rules rather than by rank.
    int rank() const;

    const char *name() const;

    std::string describe() const;   // "char *", "int [16]", for diagnostics

    // ---- struct and union ----
    const std::string &tag() const { return tag_; }
    const std::vector<Member> &members() const { return members_; }
    const Member *findMember(const std::string &name) const;

    // The one mutation the model allows. "struct Node;" names a type that a
    // pointer can already refer to, and "struct Node { ... };" completes it
    // later - which is how C describes a list that contains itself.
    void complete(std::vector<Member> members, int size, int align);

    // ---- function ----
    //
    // A function type is what it returns and what it takes. The return type
    // reuses pointee_, which reads oddly for a moment and then does not: every
    // kind here with something underneath it uses that field - a pointer's
    // pointee, an array's element, a function's result.
    //
    // The type exists in order to be pointed at. C has no object of function
    // type; a function designator converts to a pointer to one almost
    // everywhere it appears, so what the parser builds and the symbol table
    // holds is always Pointer to Function, and this is the far end of it.
    const Type *returns() const { return pointee_; }
    const std::vector<const Type *> &params() const { return params_; }
    bool isVariadicFn() const { return variadic_; }
    bool isFunction() const { return kind_ == Kind::Function; }
    // The form every actual use has.
    bool isFunctionPointer() const {
        return kind_ == Kind::Pointer && pointee_ != nullptr && pointee_->isFunction();
    }
    // "(int, char *)" or "(void)", for diagnostics and for describe().
    std::string parameterList() const;

private:
    friend class TypeTable;
    Kind kind_;
    const Type *pointee_ = nullptr;
    long length_ = -1;

    std::vector<const Type *> params_;
    bool variadic_ = false;

    std::string tag_;
    std::vector<Member> members_;
    int size_ = 0;
    int align_ = 1;
    bool complete_ = false;
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
    // Interned structurally rather than by name: two spellings of
    // "int (*)(int)" reached from different declarations are one type, which is
    // what lets a function pointer be assigned, compared and passed by the same
    // pointer-equality rule everything else here uses.
    const Type *functionType(const Type *returns,
                             std::vector<const Type *> params, bool variadic);

    // Named struct and union types are found by tag, so two mentions of
    // "struct Node" are one type. An unnamed one is never looked up again.
    Type *structType(Kind kind, const std::string &tag);
    Type *anonymousStruct(Kind kind);

    const Type *voidType() const   { return get(Kind::Void); }
    const Type *intType() const    { return get(Kind::Int); }
    const Type *doubleType() const { return get(Kind::Double); }
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
