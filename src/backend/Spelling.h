#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

// How an instruction is written down, separated from what the instruction is.
//
// The x86-64 generator selects one instruction stream for both of its targets
// - the Abi is the whole of what differs between System V and Microsoft x64 -
// but the two platforms' assemblers read different languages: GNU as takes
// AT&T, ml64 takes MASM. Those are spellings of the same instructions, and
// this interface is the seam between selecting an instruction and writing it.
//
// The generator calls these methods with its operands still carried as what
// they are - a register, an immediate, a memory reference - and the spelling
// decides sigils, operand order, brackets and size keywords. Until this seam
// existed the generator wrote AT&T text and a second pass re-parsed that text
// to respell it for ml64, which cost more time than generating it had and
// meant the compiler recognising its own output rather than knowing it.
//
// The vocabulary the generator speaks is unchanged: GNU mnemonics, register
// names with their '%'. A spelling that wants them otherwise rewrites them on
// the way out, because the one thing worse than translating text is having two
// generators agree about every instruction while spelling their names apart.

// An operand, still knowing what kind of thing it is. The text fields hold the
// generator's own vocabulary - "%rax", a displacement as it would have been
// printed - so the GNU spelling reproduces today's output byte for byte, which
// is what proved this seam changed nothing when it went in.
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
    // A view, not a string. An Op is always a temporary handed straight to
    // ins() within the same full expression, so a view over a literal - which
    // is what a register name is - or over a std::string built in the call
    // outlives the call that reads it. Holding std::string here instead cost
    // 28% of the whole compile on the gcc build: a constructor, a copy and a
    // destructor at every operand, and operands are what a code generator
    // emits most of.
    std::string_view text;
    // A memory displacement, carried as the number it is, for the same reason
    // - it was '"-" + std::to_string(off)' at every memory operand.
    long long disp = 0;
    bool hasDisp = false;
    // An immediate, likewise numeric. Magnitude and sign rather than a signed
    // value, because some of these are unsigned bit patterns past what a
    // long long holds and a pattern must not be narrowed to spell it.
    unsigned long long uimm = 0;
    bool immNeg = false;
    bool immNumeric = false;
};

inline Op reg(std::string_view r)  { return { Op::Reg, r, 0, false, 0, false, false }; }
inline Op imm(long long v) {
    Op o { Op::Imm, {}, 0, false, 0, false, true };
    if (v < 0) { o.immNeg = true; o.uimm = 0ULL - static_cast<unsigned long long>(v); }
    else o.uimm = static_cast<unsigned long long>(v);
    return o;
}
inline Op imm(unsigned long long v) { return { Op::Imm, {}, 0, false, v, false, true }; }
inline Op imm(int v)                { return imm(static_cast<long long>(v)); }
inline Op imm(unsigned int v)       { return imm(static_cast<unsigned long long>(v)); }
// An immediate already spelled - '0x0c00' stays hexadecimal because the
// comment beside it reasons in bits, and respelling it decimal would cost the
// reader that.
inline Op immText(std::string_view t) { return { Op::Imm, t, 0, false, 0, false, false }; }
// No displacement at all - '(%rsp)' - which is not the same as a displacement
// of zero, and GNU spells the two differently.
inline Op mem(std::string_view base) { return { Op::Mem, base, 0, false, 0, false, false }; }
inline Op mem(long long d, std::string_view base)
                                   { return { Op::Mem, base, d, true, 0, false, false }; }
inline Op rip(std::string_view sym) { return { Op::Rip, sym, 0, false, 0, false, false }; }
inline Op ind(std::string_view r)   { return { Op::Ind, r, 0, false, 0, false, false }; }
inline Op lbl(std::string_view l)   { return { Op::Lbl, l, 0, false, 0, false, false }; }

// Numbers appended without going through a stream. The generator emits tens of
// thousands of them and every one used to be an ostream insertion, which
// carries a sentry and a locale lookup for what is three divisions of work.
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

    // A function, as the structure it is rather than the lines it takes.
    // functionBegin covers the visibility declaration, the code section and
    // the entry label; prologue writes the frame setup *and whatever the
    // platform wants said about it* - MASM's unwind directives live inside
    // the spelling, beside the instructions they describe, so the two cannot
    // disagree. functionEnd is nothing in GNU and ENDP in MASM.
    virtual void functionBegin(const std::string &name, bool exported) = 0;
    virtual void prologue(int frameSize) = 0;
    virtual void functionEnd(const std::string &name) = 0;

    // The names this translation unit defines, told to the spelling before
    // anything is emitted. GNU has no use for it; MASM's mangling consults it
    // mid-stream, and its EXTERN block is everything referenced that was
    // never in this list.
    virtual void predefine(const std::vector<std::string> &) {}
    // Written around the collected chunks: what must precede the body, and
    // what must close the file. GNU needs neither.
    virtual void preamble(std::ostream &) {}
    virtual void postamble(std::ostream &) {}

    // The declarations and directives around the instructions. Each is one
    // fact - this symbol is visible, this data is n bytes of zeroes - stated
    // in whichever syntax the assembler reads.
    virtual void globl(const std::string &name) = 0;
    virtual void textSection() = 0;
    virtual void rodataSection() = 0;
    virtual void dataSection() = 0;
    virtual void bssSection() = 0;
    virtual void objectType(const std::string &name) = 0;
    virtual void objectSize(const std::string &name, int size) = 0;
    virtual void align(int n) = 0;
    virtual void zero(int n) = 0;
    // One initialised scalar: .byte/.word/.long/.quad by size.
    virtual void dataInt(int size, long long v) = 0;
    // An address constant: a symbol and a byte offset, always pointer-wide.
    virtual void dataSym(const std::string &sym, long long off) = 0;
    // A string literal's bytes, terminator included, exactly as given.
    virtual void dataBytes(const std::string &bytes) = 0;
};

// GNU as: AT&T operand order, '%' and '$' sigils, 'disp(base)' addressing.
// This is the vocabulary the generator itself speaks, so every method is a
// straight transcription.
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
