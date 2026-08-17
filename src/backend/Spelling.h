#pragma once

#include <string>

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
    std::string text;
    std::string disp;
};

inline Op reg(const std::string &r)  { return { Op::Reg, r, "" }; }
inline Op imm(long long v)           { return { Op::Imm, std::to_string(v), "" }; }
inline Op imm(unsigned long long v)  { return { Op::Imm, std::to_string(v), "" }; }
inline Op imm(int v)                 { return { Op::Imm, std::to_string(v), "" }; }
inline Op imm(unsigned int v)        { return { Op::Imm, std::to_string(v), "" }; }
// An immediate already spelled - '0x0c00' stays hexadecimal because the
// comment beside it reasons in bits, and respelling it decimal would cost the
// reader that.
inline Op immText(const std::string &t) { return { Op::Imm, t, "" }; }
inline Op mem(const std::string &base)  { return { Op::Mem, base, "" }; }
inline Op mem(long long d, const std::string &base)
                                     { return { Op::Mem, base, std::to_string(d) }; }
// A displacement already spelled. The one reason this exists is '-0': several
// sites print "-" and then an offset, and to_string(-off) would fold a zero
// offset to "0" where today's text says "-0".
inline Op memText(const std::string &d, const std::string &base)
                                     { return { Op::Mem, base, d }; }
inline Op rip(const std::string &sym)   { return { Op::Rip, sym, "" }; }
inline Op ind(const std::string &r)     { return { Op::Ind, r, "" }; }
inline Op lbl(const std::string &l)     { return { Op::Lbl, l, "" }; }

class Spelling {
public:
    virtual ~Spelling() = default;

    virtual void ins(const std::string &m) = 0;
    virtual void ins(const std::string &m, const Op &a) = 0;
    virtual void ins(const std::string &m, const Op &a, const Op &b) = 0;

    virtual void defLabel(const std::string &l) = 0;

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
    explicit GnuSpelling(std::ostream &o) : o_(o) {}

    void ins(const std::string &m) override;
    void ins(const std::string &m, const Op &a) override;
    void ins(const std::string &m, const Op &a, const Op &b) override;

    void defLabel(const std::string &l) override;
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
    std::ostream &o_;
    void op(const Op &x);
};
