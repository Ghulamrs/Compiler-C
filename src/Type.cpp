#include "Type.h"

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

const Type *TypeTable::arrayOf(const Type *t, long length) {
    for (Type *d : derived_)
        if (d->kind() == Kind::Array && d->pointee() == t && d->length() == length)
            return d;
    derived_.push_back(new Type(Kind::Array, t, length));
    return derived_.back();
}

static void classifyInto(const Type *t, int base, std::vector<bool> &sse,
                         const Target &target) {
    if (t->isStructOrUnion()) {
        for (const Member &m : t->members()) classifyInto(m.type, base + m.offset, sse, target);
        return;
    }
    if (t->isArray()) {
        int step = t->pointee()->size(target);
        for (long i = 0; i < t->length(); i++)
            classifyInto(t->pointee(), base + static_cast<int>(i) * step, sse, target);
        return;
    }
    if (t->isFloating()) return;

    int from = base / 8;
    int to = (base + t->size(target) - 1) / 8;
    for (int i = from; i <= to && i < static_cast<int>(sse.size()); i++) sse[i] = false;
}

std::vector<bool> classifyEightbytes(const Type *t, const Target &target) {
    int size = t->size(target);
    std::vector<bool> sse(static_cast<std::size_t>((size + 7) / 8), true);
    classifyInto(t, 0, sse, target);
    return sse;
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
    case Kind::Float:     return "float";
    case Kind::Double:    return "double";
    case Kind::Struct:    return "struct";
    case Kind::Union:     return "union";
    case Kind::Pointer:   return "pointer";
    case Kind::Array:     return "array";
    case Kind::Function:  return "function";
    }
    return "?";
}

int LinuxX86_64::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                   return 1;
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

int LinuxX86_64::alignOf(Kind k) const { return sizeOf(k); }
