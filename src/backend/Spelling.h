#pragma once

#include <cstddef>
#include <cstring>
#include <iosfwd>
#include <string>
#include <vector>

// How an instruction is written down, separated from what the instruction is.
//
// One instruction stream serves both x86-64 targets - the Abi is the whole of
// what differs - but GNU as reads AT&T and ml64 reads MASM. The generator
// calls these with its operands still knowing what they are, and the spelling
// decides sigils, operand order, brackets and size keywords. Before this seam
// the generator wrote AT&T text and a second pass re-parsed it, which cost
// more than generating it had.

// A borrowed run of characters. This is std::string_view under another name,
// and exists because the compiler is built as C++14, where that type is not
// there yet. Only the four things an Op does with its text are here - it is
// not a string class, and nothing should grow it into one.
struct Str {
    const char *p = "";
    std::size_t n = 0;

    Str() = default;
    // Both implicit, so a caller writes reg("%rax") and mem(base) exactly as
    // it did when this was a view.
    Str(const char *s) : p(s), n(std::strlen(s)) {}
    Str(const std::string &s) : p(s.data()), n(s.size()) {}
    Str(const char *s, std::size_t len) : p(s), n(len) {}

    // From 'from' to the end, which is the only slice anything asks for.
    Str substr(std::size_t from) const { return Str(p + from, n - from); }
    bool startsWith(const char *s) const {
        std::size_t m = std::strlen(s);
        return n >= m && std::memcmp(p, s, m) == 0;
    }
    // Explicit, as std::string_view's conversion is: this one allocates and
    // copies, and that should be visible at the place it happens.
    explicit operator std::string() const { return std::string(p, n); }
};

inline std::string &operator+=(std::string &s, Str v) {
    return s.append(v.p, v.n);
}

// An operand, still knowing what kind of thing it is.
struct Op {
    enum Kind {
        Reg,   // a register: text = "%rax"
        Imm,   // an immediate: text = the value's spelling, without the '$'
        Mem,   // memory: disp = displacement or empty, text = the base register
        Rip,   // rip-relative: text = the symbol
        Ind,   // an indirect call target: text = the register holding it
        Lbl,   // a label used as an operand: a jump target, a call target
    };
    Kind kind;
    // A view, not a string: an Op is a temporary handed straight to ins(), so it
    // borrows its text rather than copying it.
    Str text;
    long long disp = 0;
    bool hasDisp = false;
    // Magnitude and sign rather than a signed value, so the whole unsigned range
    // survives.
    unsigned long long uimm = 0;
    bool immNeg = false;
    bool immNumeric = false;
};

inline Op reg(Str r)  { return { Op::Reg, r, 0, false, 0, false, false }; }
inline Op imm(long long v) {
    Op o { Op::Imm, {}, 0, false, 0, false, true };
    if (v < 0) { o.immNeg = true; o.uimm = 0ULL - static_cast<unsigned long long>(v); }
    else o.uimm = static_cast<unsigned long long>(v);
    return o;
}
inline Op imm(unsigned long long v) { return { Op::Imm, {}, 0, false, v, false, true }; }
inline Op imm(int v)                { return imm(static_cast<long long>(v)); }
inline Op imm(unsigned int v)       { return imm(static_cast<unsigned long long>(v)); }
// '0x0c00' stays hexadecimal because the comment beside it reasons in bits.
inline Op immText(Str t) { return { Op::Imm, t, 0, false, 0, false, false }; }
// No displacement at all - '(%rsp)' - which is not a displacement of zero.
inline Op mem(Str base) { return { Op::Mem, base, 0, false, 0, false, false }; }
inline Op mem(long long d, Str base)
                                   { return { Op::Mem, base, d, true, 0, false, false }; }
inline Op rip(Str sym) { return { Op::Rip, sym, 0, false, 0, false, false }; }
inline Op ind(Str r)   { return { Op::Ind, r, 0, false, 0, false, false }; }
inline Op lbl(Str l)   { return { Op::Lbl, l, 0, false, 0, false, false }; }

// Appended without going through a stream: the generator emits tens of
// thousands of these per unit.
inline void appendNum(std::string &s, unsigned long long v) {
    char b[24];
    int i = 24;
    if (v == 0) b[--i] = '0';
    while (v != 0) { b[--i] = static_cast<char>('0' + v % 10); v /= 10; }
    s.append(b + i, static_cast<std::size_t>(24 - i));
}
inline void appendNum(std::string &s, long long v) {
    if (v < 0) { s += '-'; appendNum(s, 0ULL - static_cast<unsigned long long>(v)); }
    else appendNum(s, static_cast<unsigned long long>(v));
}
inline void appendNum(std::string &s, int v) { appendNum(s, static_cast<long long>(v)); }

class Spelling {
public:
    virtual ~Spelling() = default;

    virtual void ins(const std::string &m) = 0;
    virtual void ins(const std::string &m, const Op &a) = 0;
    virtual void ins(const std::string &m, const Op &a, const Op &b) = 0;

    virtual void defLabel(const std::string &l) = 0;

    // prologue() writes the frame setup *and* whatever the platform wants said
    // about it - MASM's unwind directives live beside the instructions they
    // describe, so the two cannot disagree.
    virtual void functionBegin(const std::string &name, bool exported) = 0;
    virtual void prologue(int frameSize) = 0;
    virtual void functionEnd(const std::string &name) = 0;

    // GNU has no use for it; MASM's mangling consults it mid-stream.
    virtual void predefine(const std::vector<std::string> &) {}
    virtual void preamble(std::ostream &) {}
    virtual void postamble(std::ostream &) {}

    virtual void globl(const std::string &name) = 0;
    virtual void textSection() = 0;
    virtual void rodataSection() = 0;
    virtual void dataSection() = 0;
    virtual void bssSection() = 0;
    virtual void objectType(const std::string &name) = 0;
    virtual void objectSize(const std::string &name, int size) = 0;
    virtual void align(int n) = 0;
    virtual void zero(int n) = 0;
    virtual void dataInt(int size, long long v) = 0;
    // An address constant: a symbol and a byte offset, always pointer-wide.
    virtual void dataSym(const std::string &sym, long long off) = 0;
    virtual void dataBytes(const std::string &bytes) = 0;
};

// GNU as: the vocabulary the generator itself speaks, so every method here is
// a straight transcription.
class GnuSpelling final : public Spelling {
public:
    explicit GnuSpelling(std::string &o) : o_(o) {}

    void ins(const std::string &m) override;
    void ins(const std::string &m, const Op &a) override;
    void ins(const std::string &m, const Op &a, const Op &b) override;

    void defLabel(const std::string &l) override;
    void functionBegin(const std::string &name, bool exported) override;
    void prologue(int frameSize) override;
    void functionEnd(const std::string &name) override;
    void globl(const std::string &name) override;
    void textSection() override;
    void rodataSection() override;
    void dataSection() override;
    void bssSection() override;
    void objectType(const std::string &name) override;
    void objectSize(const std::string &name, int size) override;
    void align(int n) override;
    void zero(int n) override;
    void dataInt(int size, long long v) override;
    void dataSym(const std::string &sym, long long off) override;
    void dataBytes(const std::string &bytes) override;

private:
    std::string &o_;
    void op(const Op &x);
};
