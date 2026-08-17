#include "Spelling.h"

#include <ostream>

// The GNU spelling is the generator's own vocabulary written out verbatim, and
// that is load-bearing rather than convenient: when this seam was cut into a
// generator that had always written text directly, the proof that nothing had
// changed was a byte-for-byte comparison of every file the corpus compiles to.
// Each method here holds the exact spacing the inline text had, because the
// comparison is only worth anything while nothing is normalised.

void GnuSpelling::op(const Op &x) {
    switch (x.kind) {
    case Op::Reg: o_ << x.text; return;
    case Op::Imm: o_ << '$' << x.text; return;
    case Op::Mem: o_ << x.disp << '(' << x.text << ')'; return;
    case Op::Rip: o_ << x.text << "(%rip)"; return;
    case Op::Ind: o_ << '*' << x.text; return;
    case Op::Lbl: o_ << x.text; return;
    }
}

void GnuSpelling::ins(const std::string &m) { o_ << "  " << m << "\n"; }

void GnuSpelling::ins(const std::string &m, const Op &a) {
    o_ << "  " << m << ' ';
    op(a);
    o_ << "\n";
}

void GnuSpelling::ins(const std::string &m, const Op &a, const Op &b) {
    o_ << "  " << m << ' ';
    op(a);
    o_ << ", ";
    op(b);
    o_ << "\n";
}

void GnuSpelling::defLabel(const std::string &l) { o_ << l << ":\n"; }

void GnuSpelling::globl(const std::string &name) {
    o_ << "  .globl " << name << "\n";
}

void GnuSpelling::textSection()   { o_ << "  .text\n"; }
void GnuSpelling::rodataSection() { o_ << "  .section .rodata\n"; }
void GnuSpelling::dataSection()   { o_ << "  .data\n"; }
void GnuSpelling::bssSection()    { o_ << "  .bss\n"; }

void GnuSpelling::objectType(const std::string &name) {
    o_ << "  .type " << name << ", @object\n";
}

void GnuSpelling::objectSize(const std::string &name, int size) {
    o_ << "  .size " << name << ", " << size << "\n";
}

void GnuSpelling::align(int n) { o_ << "  .align " << n << "\n"; }
void GnuSpelling::zero(int n)  { o_ << "  .zero " << n << "\n"; }

void GnuSpelling::dataInt(int size, long long v) {
    switch (size) {
    case 1: o_ << "  .byte "  << v << "\n"; break;
    case 2: o_ << "  .word "  << v << "\n"; break;
    case 4: o_ << "  .long "  << v << "\n"; break;
    default: o_ << "  .quad " << v << "\n"; break;
    }
}

void GnuSpelling::dataSym(const std::string &sym, long long off) {
    o_ << "  .quad " << sym;
    if (off > 0) o_ << "+" << off;
    else if (off < 0) o_ << "-" << -off;
    o_ << "\n";
}

void GnuSpelling::dataBytes(const std::string &bytes) {
    o_ << "  .byte ";
    for (std::size_t k = 0; k < bytes.size(); k++) {
        if (k) o_ << ", ";
        o_ << static_cast<int>(static_cast<unsigned char>(bytes[k]));
    }
    o_ << "\n";
}
