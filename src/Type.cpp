#include "Type.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

TypeTable::TypeTable() {
    for (int k = static_cast<int>(Kind::Void);
         k <= static_cast<int>(Kind::Function); k++)
        types_.push_back(Type(static_cast<Kind>(k)));
}

const Type *TypeTable::get(Kind k) const {
    return &types_[static_cast<std::size_t>(k)];
}

int Type::size(const Target &t) const {
    if (kind_ == Kind::Array) return static_cast<int>(length_) * pointee_->size(t);
    if (kind_ == Kind::Struct || kind_ == Kind::Union) return size_;
    return t.sizeOf(kind_);
}

int Type::align(const Target &t) const {
    if (kind_ == Kind::Array) return pointee_->align(t);
    if (kind_ == Kind::Struct || kind_ == Kind::Union) return align_;
    return t.alignOf(kind_);
}

std::string Type::parameterList() const {
    std::string s = "(";
    for (std::size_t i = 0; i < params_.size(); i++)
        s += (i ? ", " : "") + params_[i]->describe();
    if (variadic_) s += params_.empty() ? "..." : ", ...";
    if (params_.empty() && !variadic_) s += "void";
    return s + ")";
}

std::string Type::describe() const {
    if (kind_ == Kind::Pointer && pointee_->isFunction())
        return pointee_->returns()->describe() + " (*)" + pointee_->parameterList();
    if (kind_ == Kind::Function)
        return pointee_->describe() + " " + parameterList();
    if (kind_ == Kind::Pointer) return pointee_->describe() + " *";
    if (kind_ == Kind::Array)
        return pointee_->describe() + " [" + std::to_string(length_) + "]";
    if (kind_ == Kind::Struct) return "struct " + (tag_.empty() ? "<anonymous>" : tag_);
    if (kind_ == Kind::Union)  return "union "  + (tag_.empty() ? "<anonymous>" : tag_);
    return name();
}

const Type *TypeTable::functionType(const Type *returns,
                                    std::vector<const Type *> params,
                                    bool variadic) {
    for (Type *d : derived_)
        if (d->kind() == Kind::Function && d->pointee() == returns &&
            d->variadic_ == variadic && d->params_ == params)
            return d;
    Type *t = new Type(Kind::Function, returns, -1);
    t->params_ = std::move(params);
    t->variadic_ = variadic;
    derived_.push_back(t);
    return derived_.back();
}

const Type *TypeTable::pointerTo(const Type *t) {
    for (Type *d : derived_)
        if (d->kind() == Kind::Pointer && d->pointee() == t) return d;
    derived_.push_back(new Type(Kind::Pointer, t, -1));
    return derived_.back();
}

const Type *TypeTable::arrayOf(const Type *t, long long length) {
    for (Type *d : derived_)
        if (d->kind() == Kind::Array && d->pointee() == t && d->length() == length)
            return d;
    derived_.push_back(new Type(Kind::Array, t, length));
    return derived_.back();
}

const Member *Type::findMember(const std::string &name) const {
    for (const Member &m : members_)
        if (m.name == name) return &m;
    return nullptr;
}

void Type::complete(std::vector<Member> members, int size, int align) {
    members_ = std::move(members);
    size_ = size;
    align_ = align;
    complete_ = true;
}

Type *TypeTable::structType(Kind kind, const std::string &tag) {
    for (Type *d : derived_)
        if (d->kind() == kind && d->tag_ == tag) return d;
    Type *t = new Type(kind);
    t->tag_ = tag;
    derived_.push_back(t);
    return t;
}

Type *TypeTable::anonymousStruct(Kind kind) {
    Type *t = new Type(kind);
    derived_.push_back(t);
    return t;
}

bool Type::isSigned(const Target &t) const {
    switch (kind_) {
    case Kind::Char:      return t.plainCharIsSigned();
    case Kind::SChar:
    case Kind::Short:
    case Kind::Int:
    case Kind::Long:
    case Kind::LongLong:  return true;
    default:              return false;
    }
}

int Type::rank() const {
    switch (kind_) {
    case Kind::Char: case Kind::SChar: case Kind::UChar:       return 1;
    case Kind::Short: case Kind::UShort:                       return 2;
    case Kind::Int: case Kind::UInt:                           return 3;
    case Kind::Long: case Kind::ULong:                         return 4;
    case Kind::LongLong: case Kind::ULongLong:                 return 5;
    case Kind::Float:                                          return 6;
    case Kind::Double:                                         return 7;
    case Kind::LongDouble:                                     return 8;
    default:                                                   return 0;
    }
}

bool Type::isX87(const Target &t) const {
    return kind_ == Kind::LongDouble && t.sizeOf(Kind::LongDouble) > 8;
}

void x87Parts(long double v, unsigned long long *significand, unsigned int *signExp) {

    bool neg = std::signbit(v);
    if (neg) v = -v;

    unsigned long long sig = 0;
    unsigned int expField = 0;

    if (std::isnan(v)) {
        expField = 0x7fff;
        sig = 0xc000000000000000ULL;
    } else if (std::isinf(v)) {
        expField = 0x7fff;
        sig = 0x8000000000000000ULL;
    } else if (v != 0) {
        int e = 0;
        long double m = std::frexp(v, &e);
        sig = static_cast<unsigned long long>(std::ldexp(m, 64));
        expField = static_cast<unsigned int>(e - 1 + 16383) & 0x7fffu;
    }

    *significand = sig;
    *signExp = (neg ? 0x8000u : 0u) | expField;
}

int objectAlign(const Type *t, const Target &target) {
    int a = t->align(target);
    if (t->size(target) >= 16 && a < 16) a = 16;
    return a;
}

bool containsX87(const Type *t, const Target &target) {
    if (t == nullptr) return false;
    if (t->isX87(target)) return true;
    if (t->isArray()) return containsX87(t->pointee(), target);
    if (t->isStructOrUnion())
        for (const Member &m : t->members())
            if (containsX87(m.type, target)) return true;
    return false;
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
    case Kind::Float:     return "float";
    case Kind::Double:    return "double";
    case Kind::LongDouble: return "long double";
    case Kind::Struct:    return "struct";
    case Kind::Union:     return "union";
    case Kind::Pointer:   return "pointer";
    case Kind::Array:     return "array";
    case Kind::Function:  return "function";
    }
    return "?";
}

static Kind hfaElem(const Type *t) {
    return t->kind() == Kind::LongDouble ? Kind::Double : t->kind();
}

static int hfaWalk(const Type *t, Kind *elem, bool *set) {
    if (t == nullptr) return 0;
    if (t->isFloating()) {
        if (!*set) { *elem = hfaElem(t); *set = true; }
        else if (*elem != hfaElem(t)) return 0;
        return 1;
    }
    if (t->kind() == Kind::Array) {
        if (t->length() <= 0) return 0;
        int one = hfaWalk(t->pointee(), elem, set);
        if (one == 0) return 0;
        long long total = one * t->length();
        return total > 4 ? 5 : static_cast<int>(total);
    }
    if (t->isStructOrUnion()) {

        bool isUnion = t->kind() == Kind::Union;
        int total = 0;
        for (const Member &m : t->members()) {
            if (m.isBitField()) return 0;
            int one = hfaWalk(m.type, elem, set);
            if (one == 0) return 0;
            if (isUnion) total = one > total ? one : total;
            else         total += one;
            if (total > 4) return 5;
        }
        return total;
    }
    return 0;
}

int homogeneousFloatCount(const Type *t, Kind *elem) {
    if (t == nullptr || !t->isStructOrUnion()) return 0;
    if (t->members().empty()) return 0;
    Kind k = Kind::Double;
    bool set = false;
    int n = hfaWalk(t, &k, &set);
    if (!set || n < 1 || n > 4) return 0;
    *elem = k;
    return n;
}
