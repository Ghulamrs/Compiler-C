#include "Spelling.h"

#include <ostream>

// The GNU spelling is the generator's own vocabulary written out verbatim, and
// that is load-bearing rather than convenient: when this seam was cut into a
// generator that had always written text directly, the proof that nothing had
// changed was a byte-for-byte comparison of every file the corpus compiles to.
// Each method here holds the exact spacing the inline text had, because the
// comparison is only worth anything while nothing is normalised.
//
// It appends to a string rather than inserting into a stream. The seam roughly
// doubled the number of writes per instruction - a mnemonic, a sigil, an
// operand, a comma - and an ostream insertion is far more than an append.

void GnuSpelling::op(const Op &x) {
    switch (x.kind) {
    case Op::Reg: o_ += x.text; return;
    case Op::Imm:
        o_ += '$';
        if (!x.immNumeric) { o_ += x.text; return; }
        if (x.immNeg) o_ += '-';
        appendNum(o_, x.uimm);
        return;
    case Op::Mem:
        if (x.hasDisp) appendNum(o_, x.disp);
        o_ += '(';
        o_ += x.text;
        o_ += ')';
        return;
    case Op::Rip: o_ += x.text; o_ += "(%rip)"; return;
    case Op::Ind: o_ += '*'; o_ += x.text; return;
    case Op::Lbl: o_ += x.text; return;
    }
}

void GnuSpelling::ins(const std::string &m) { o_ += "  "; o_ += m; o_ += '\n'; }

void GnuSpelling::ins(const std::string &m, const Op &a) {
    o_ += "  "; o_ += m; o_ += ' ';
    op(a);
    o_ += '\n';
}

void GnuSpelling::ins(const std::string &m, const Op &a, const Op &b) {
    o_ += "  "; o_ += m; o_ += ' ';
    op(a);
    o_ += ", ";
    op(b);
    o_ += '\n';
}

void GnuSpelling::defLabel(const std::string &l) { o_ += l; o_ += ":\n"; }

void GnuSpelling::functionBegin(const std::string &name, bool exported) {
    if (exported) globl(name);
    textSection();
    defLabel(name);
}

void GnuSpelling::prologue(int frameSize) {
    ins("push", reg("%rbp"));
    ins("mov", reg("%rsp"), reg("%rbp"));
    if (frameSize > 0) ins("sub", imm(frameSize), reg("%rsp"));
}

void GnuSpelling::functionEnd(const std::string &) {}

void GnuSpelling::globl(const std::string &name) {
    o_ += "  .globl "; o_ += name; o_ += '\n';
}

void GnuSpelling::textSection()   { o_ += "  .text\n"; }
void GnuSpelling::rodataSection() { o_ += "  .section .rodata\n"; }
void GnuSpelling::dataSection()   { o_ += "  .data\n"; }
void GnuSpelling::bssSection()    { o_ += "  .bss\n"; }

void GnuSpelling::objectType(const std::string &name) {
    o_ += "  .type "; o_ += name; o_ += ", @object\n";
}

void GnuSpelling::objectSize(const std::string &name, int size) {
    o_ += "  .size "; o_ += name; o_ += ", "; appendNum(o_, size); o_ += '\n';
}

void GnuSpelling::align(int n) { o_ += "  .align "; appendNum(o_, n); o_ += '\n'; }
void GnuSpelling::zero(int n)  { o_ += "  .zero ";  appendNum(o_, n); o_ += '\n'; }

void GnuSpelling::dataInt(int size, long long v) {
    switch (size) {
    case 1: o_ += "  .byte "; break;
    case 2: o_ += "  .word "; break;
    case 4: o_ += "  .long "; break;
    default: o_ += "  .quad "; break;
    }
    appendNum(o_, v);
    o_ += '\n';
}

void GnuSpelling::dataSym(const std::string &sym, long long off) {
    o_ += "  .quad ";
    o_ += sym;
    if (off > 0) { o_ += '+'; appendNum(o_, off); }
    else if (off < 0) { o_ += '-'; appendNum(o_, -off); }
    o_ += '\n';
}

void GnuSpelling::dataBytes(const std::string &bytes) {
    o_ += "  .byte ";
    for (std::size_t k = 0; k < bytes.size(); k++) {
        if (k) o_ += ", ";
        appendNum(o_, static_cast<long long>(
                          static_cast<unsigned char>(bytes[k])));
    }
    o_ += '\n';
}
