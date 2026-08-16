#pragma once

#include <string>
#include <vector>

enum class Kind {
    Void,
    Char, SChar, UChar,
    Short, UShort,
    Int, UInt,
    Long, ULong,
    LongLong, ULongLong,
    Float, Double, LongDouble,
    Struct, Union,
    Pointer, Array, Function
};

class Target;

class Type;

// AAPCS64's Homogeneous Floating-point Aggregate: a struct or union whose
// members are all the same floating type, counting through nested aggregates
// and arrays, with no more than four of them in total. Such an aggregate
// travels in consecutive vector registers rather than being cut into
// eightbytes - so 'struct { double a, b; }' goes in d0 and d1, where System V
// would also use two vector registers but for a different reason, and where
// Microsoft x64 would pass a pointer to a copy.
//
// Returns the member count, 1 to 4, or 0 for anything that is not one. The
// element kind comes back through 'elem' when it is.
int homogeneousFloatCount(const Type *t, Kind *elem);

// Whether an x87 long double is anywhere inside this type, however deeply. An
// aggregate holding one is MEMORY under System V whatever its size and is
// returned through the hidden pointer rather than in registers - and the
// parser has to know that, because the frame slot for that pointer is its to
// allocate. False on every target where long double is double.
bool containsX87(const Type *t, const Target &target);

// x87's 80-bit format taken apart: a 64-bit significand carrying its leading
// one explicitly, and a sixteen-bit field holding the sign and the exponent
// biased by 16383. Built with frexpl rather than copied out of the host's own
// long double, because the host need not have this format - a compiler built
// on Apple's arm64 has a 64-bit long double, and its bytes are a double's.
// Two callers need it and must agree: the constant loaded into a register, and
// the one laid down as data for a file-scope initialiser.
void x87Parts(long double v, unsigned long *significand, unsigned int *signExp);

// How much alignment an *object* gets, as against what its type requires. Any
// object of sixteen bytes or more is given sixteen, which no C90 type asks for
// - eight is the widest alignment here - but which the platform underneath
// does: the UCRT's jmp_buf is filled with aligned xmm saves and faults on an
// odd address, and aligned SSE moves are pointed at buffers generally.
//
// This governs frame slots and file-scope objects only. Struct member offsets
// and argument slots are ABI and are laid out where the platform says, so they
// keep asking the type rather than this.
int objectAlign(const Type *t, const Target &target);

struct Member {
    std::string name;
    const Type *type;
    int offset;
    int width = 0;
    int bitOffset = 0;

    bool isBitField() const { return width != 0; }
};

class Type {
public:
    explicit Type(Kind k) : kind_(k) {}
    Type(Kind k, const Type *pointee, long length)
        : kind_(k), pointee_(pointee), length_(length) {}

    Kind kind() const { return kind_; }

    const Type *pointee() const { return pointee_; }
    long length() const { return length_; }

    bool isPointer() const { return kind_ == Kind::Pointer; }
    bool isArray() const { return kind_ == Kind::Array; }
    bool isScalar() const { return isArithmetic() || isPointer(); }

    bool isInteger() const {
        return kind_ >= Kind::Char && kind_ <= Kind::ULongLong;
    }
    bool isFloating() const {
        return kind_ >= Kind::Float && kind_ <= Kind::LongDouble;
    }

    // Whether this type is x87's 80-bit format rather than an SSE one. It is a
    // question about the target and not about the type: 'long double' is the
    // 80-bit extended format on System V, and is plain double under both the
    // UCRT and Apple's arm64, where the two spellings name one machine type.
    // Everything the code generators do differently for it hangs off this.
    bool isX87(const Target &t) const;
    bool isArithmetic() const { return isInteger() || isFloating(); }
    bool isVoid() const { return kind_ == Kind::Void; }
    bool isStructOrUnion() const { return kind_ == Kind::Struct || kind_ == Kind::Union; }
    bool isComplete() const {
        if (isVoid()) return false;
        if (isArray() && length_ < 0) return false;
        if (isStructOrUnion()) return complete_;
        return true;
    }

    int size(const Target &t) const;
    int align(const Target &t) const;
    bool isSigned(const Target &t) const;

    int rank() const;

    const char *name() const;

    std::string describe() const;

    const std::string &tag() const { return tag_; }
    const std::vector<Member> &members() const { return members_; }
    const Member *findMember(const std::string &name) const;

    void complete(std::vector<Member> members, int size, int align);

    const Type *returns() const { return pointee_; }
    const std::vector<const Type *> &params() const { return params_; }
    bool isVariadicFn() const { return variadic_; }
    bool isFunction() const { return kind_ == Kind::Function; }
    bool isFunctionPointer() const {
        return kind_ == Kind::Pointer && pointee_ != nullptr && pointee_->isFunction();
    }
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

class TypeTable {
public:
    TypeTable();
    const Type *get(Kind k) const;

    const Type *pointerTo(const Type *t);
    const Type *arrayOf(const Type *t, long length);
    const Type *functionType(const Type *returns,
                             std::vector<const Type *> params, bool variadic);

    Type *structType(Kind kind, const std::string &tag);
    Type *anonymousStruct(Kind kind);

    const Type *voidType() const   { return get(Kind::Void); }
    const Type *intType() const    { return get(Kind::Int); }
    const Type *doubleType() const { return get(Kind::Double); }
    const Type *charType() const   { return get(Kind::Char); }

private:
    std::vector<Type> types_;
    std::vector<Type *> derived_;
};

class Target {
public:
    virtual ~Target() = default;

    virtual int sizeOf(Kind) const = 0;
    virtual int alignOf(Kind) const = 0;
    virtual bool plainCharIsSigned() const = 0;

    virtual Kind sizeType() const = 0;

    // What L'x' and L"..." are made of. Measured on each platform rather than
    // assumed, because the three do not agree and the disagreement shows in a
    // program: int on Linux and macOS, four bytes and signed, against unsigned
    // short under the UCRT, two bytes and not. So sizeof(L"hi") is 12 on two of
    // these targets and 6 on the third.
    virtual Kind wcharType() const = 0;

    virtual const char *name() const = 0;
};
