#include "Masm.h"

#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void give_up(const std::string &what, const std::string &why) {
    std::fprintf(stderr, "cc1: masm: %s\n  for: %s\n", why.c_str(),
                 what.c_str());
    std::exit(1);
}

// MASM will not take a register, a mnemonic or one of its own directives as an
// identifier, and C has no such reservations - a global called 'gs' or a
// function called 'add' is good C that ml64 refuses. Such a name defined here
// gets a '$' in front. Dotted labels the generator invents get one too, '.'
// beginning a directive.
bool isReservedInMasm(const std::string &name) {
    static const char *const kReserved[] = {
        "ah","al","ax","eax","rax","bh","bl","bx","ebx","rbx",
        "ch","cl","cx","ecx","rcx","dh","dl","dx","edx","rdx",
        "si","esi","rsi","di","edi","rdi","bp","ebp","rbp","sp","esp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "cs","ds","es","fs","gs","ss","st","flat","eip","rip",
        "byte","word","dword","qword","tbyte","oword","ptr","offset",
        "length","lengthof","size","sizeof","type","typedef","this",
        "near","far","short","proc","endp","segment","ends","assume",
        "public","extern","externdef","end","align","even","org","dup",
        "mask","width","low","high","lowword","highword",
        "and","or","xor","not","mod","shl","shr","eq","ne","lt","le","gt","ge",
        "if","else","endif","macro","endm","rept","irp","exitm","local",
        "label","comment","include","includelib","name","group","record",
        "struc","struct","union","db","dw","dd","dq","dt","page","title",
        // Instruction mnemonics. 'add', 'sub', 'mul' and 'in' are ordinary C names.
        "aaa","aad","aam","aas","adc","add","bound","bsf","bsr","bt","btc",
        "btr","bts","call","cbw","cdq","clc","cld","cli","cmc","cmp","cmps",
        "cmpsb","cmpsd","cmpsw","cqo","cwd","cwde","daa","das","dec","div",
        "enter","esc","hlt","idiv","imul","in","inc","ins","int","into","iret",
        "ja","jae","jb","jbe","jc","jcxz","je","jg","jge","jl","jle","jmp",
        "jna","jnb","jnc","jne","jng","jnl","jno","jnp","jns","jnz","jo","jp",
        "jpe","jpo","js","jz","lahf","lds","lea","leave","les","lock","lods",
        "lodsb","lodsd","lodsw","loop","loope","loopne","loopnz","loopz",
        "mov","movs","movsb","movsd","movss","movsw","movsx","movsxd","movzx",
        "mul","neg","nop","out","outs","pop","popa","popf","push","pusha",
        "pushf","rcl","rcr","rep","repe","repne","repnz","repz","ret","retf",
        "retn","rol","ror","sahf","sal","sar","sbb","scas","scasb","scasd",
        "scasw","seta","setae","setb","setbe","sete","setg","setge","setl",
        "setle","setna","setne","setnz","setz","sgdt","shld","shrd","sidt",
        "stc","std","sti","stos","stosb","stosd","stosw","sub","test","wait",
        "xadd","xchg","xlat","xlatb",
        "fabs","fadd","fchs","fcos","fdiv","fild","finit","fist","fld","fmul",
        "fnop","fprem","fptan","frndint","fscale","fsin","fsqrt","fst","fstp",
        "fsub","ftst","fwait","fxam","fxch",
        "addpd","addps","addsd","addss","andpd","andps","cvtsd2ss","cvtsi2sd",
        "cvtss2sd","cvttsd2si","divsd","divss","maxsd","minsd","movapd","movaps",
        "movd","movq","mulsd","mulss","orpd","orps","pxor","sqrtsd","subsd",
        "subss","ucomisd","ucomiss","xorpd","xorps",
        nullptr,
    };
    std::string lower;
    for (char c : name) lower += static_cast<char>(std::tolower(c));
    for (const char *const *p = kReserved; *p != nullptr; ++p)
        if (lower == *p) return true;
    return false;
}

// The '$' is a private spelling and stops at the edge of what this unit
// defines: the linker looks for 'fabs' because that is what the UCRT exports,
// and '$fabs' resolves to nothing - which cost lib_math its link.
//
// OPTION NOKEYWORD gives a word back for the whole file, so it can only be
// used for one cc1 never writes itself: un-reserving 'add' turns this file's
// own 'add rsp, 40' into a syntax error, checked on ml64 rather than assumed.
// The x87 group qualifies because Windows makes long double another spelling
// of double and this path emits no x87 at all - verified by intersecting the
// whole corpus's output with the list above. Anything else is refused by name;
// 'div' from <stdlib.h> is the real instance.
bool canUnreserve(const std::string &name) {
    static const char *const kNeverEmitted[] = {
        "fabs","fadd","fchs","fcos","fdiv","fild","finit","fist","fld","fmul",
        "fnop","fprem","fptan","frndint","fscale","fsin","fsqrt","fst","fstp",
        "fsub","ftst","fwait","fxam","fxch",
        nullptr,
    };
    std::string lower;
    for (char c : name) lower += static_cast<char>(std::tolower(c));
    for (const char *const *p = kNeverEmitted; *p != nullptr; ++p)
        if (lower == *p) return true;
    return false;
}

const char *ptrFor(int bytes) {
    switch (bytes) {
    case 1: return "BYTE PTR ";
    case 2: return "WORD PTR ";
    case 4: return "DWORD PTR ";
    case 8: return "QWORD PTR ";
    default: return "";
    }
}

// Anything outside this set stops the compiler rather than being guessed at.
struct Rule {
    const char *att;     // what the generator's vocabulary calls it
    const char *masm;    // what ml64 reads
    int width;           // bytes a memory operand carries, 0 when a register
                         // operand already says
};

const Rule kRules[] = {
    { "movb", "mov", 1 }, { "movw", "mov", 2 },
    { "movl", "mov", 4 }, { "movq", "mov", 8 },
    { "mov",  "mov", 0 }, { "movabs", "mov", 0 },
    { "movslq", "movsxd", 4 },
    { "movsbq", "movsx", 1 }, { "movswq", "movsx", 2 },
    // movzwq arrived with wchar_t: 'unsigned short' here and 'int' on the other
    // two targets, so nothing had asked for that instruction before.
    { "movzbq", "movzx", 1 }, { "movzwq", "movzx", 2 },
    { "movzbl", "movzx", 1 }, { "movzwl", "movzx", 2 },

    { "lea", "lea", 0 },
    { "push", "push", 0 }, { "pop", "pop", 0 },

    { "add", "add", 0 }, { "sub", "sub", 0 }, { "imul", "imul", 0 },
    { "idiv", "idiv", 0 }, { "div", "div", 0 }, { "neg", "neg", 0 },
    { "and", "and", 0 }, { "or", "or", 0 }, { "xor", "xor", 0 },
    { "shl", "shl", 0 }, { "shr", "shr", 0 }, { "sar", "sar", 0 },
    { "cmp", "cmp", 0 }, { "cdq", "cdq", 0 }, { "cqo", "cqo", 0 },
    { "addl", "add", 4 }, { "cmpl", "cmp", 4 }, { "testb", "test", 1 },

    { "call", "call", 0 }, { "ret", "ret", 0 },
    { "jmp", "jmp", 0 }, { "je", "je", 0 }, { "jne", "jne", 0 },
    { "jae", "jae", 0 }, { "jns", "jns", 0 },

    { "sete", "sete", 0 }, { "setne", "setne", 0 },
    { "setl", "setl", 0 }, { "setle", "setle", 0 },
    { "setg", "setg", 0 }, { "setge", "setge", 0 },
    { "seta", "seta", 0 }, { "setae", "setae", 0 },
    { "setb", "setb", 0 }, { "setbe", "setbe", 0 },
    { "setp", "setp", 0 }, { "setnp", "setnp", 0 },

    // SSE. Same mnemonics either side; only the operand order turns over.
    { "movsd", "movsd", 8 }, { "movss", "movss", 4 },
    { "movapd", "movapd", 0 }, { "movaps", "movaps", 0 },
    { "movd", "movd", 0 },
    { "addsd", "addsd", 0 }, { "subsd", "subsd", 0 },
    { "mulsd", "mulsd", 0 }, { "divsd", "divsd", 0 },
    { "addss", "addss", 0 }, { "subss", "subss", 0 },
    { "mulss", "mulss", 0 }, { "divss", "divss", 0 },
    { "ucomisd", "ucomisd", 0 }, { "ucomiss", "ucomiss", 0 },
    { "pxor", "pxor", 0 }, { "xorpd", "xorpd", 0 }, { "xorps", "xorps", 0 },
    { "cvtsi2sdq", "cvtsi2sd", 8 }, { "cvtsi2ssq", "cvtsi2ss", 8 },
    { "cvttsd2si", "cvttsd2si", 0 }, { "cvttss2si", "cvttss2si", 0 },
    { "cvtss2sd", "cvtss2sd", 0 }, { "cvtsd2ss", "cvtsd2ss", 0 },
};

// x87 is deliberately absent. Windows makes 'long double' another spelling of
// 'double', so an x87 instruction reaching here is a bug in the generator and
// give_up is the right answer rather than a rule that hides it.

const Rule *ruleFor(const std::string &m) {
    for (const Rule &r : kRules)
        if (m == r.att) return &r;
    return nullptr;
}

}  // namespace

// Dotted internal labels get '$' and underscores; a defined reserved word gets
// its '$'. An imported one keeps the spelling the linker knows it by.
std::string MasmSpelling::mangle(const std::string &name) {
    if (name.find('.') != std::string::npos) {
        std::string out = "$";
        for (char c : name) out += (c == '.') ? '_' : c;
        return out;
    }
    if (!isReservedInMasm(name)) return name;
    if (defined_.find(name) != defined_.end()) return "$" + name;
    if (!canUnreserve(name))
        give_up(name, "'" + name + "' is a name ml64 reserves and this file "
                      "emits, so an imported symbol cannot be spelled it");
    unreserved_.insert(name);
    return name;
}

MasmSpelling::Rendered MasmSpelling::render(const Op &x) {
    switch (x.kind) {
    case Op::Reg:
        return { std::string(x.text.substr(1)), false, false,
                 x.text.compare(1, 3, "xmm") == 0 };
    case Op::Imm: {
        if (!x.immNumeric) return { std::string(x.text), false, true, false };
        std::string v = x.immNeg ? "-" + std::to_string(x.uimm)
                                 : std::to_string(x.uimm);
        return { v, false, true, false };
    }
    case Op::Mem: {
        // '-4(%rbp)' is '[rbp-4]'. A zero displacement is not written.
        std::string t = "[" + std::string(x.text.substr(1));
        if (x.hasDisp && x.disp != 0) {
            if (x.disp < 0) t += std::to_string(x.disp);
            else            t += "+" + std::to_string(x.disp);
        }
        t += "]";
        return { t, true, false, false };
    }
    case Op::Rip: {
        // ml64 makes a named reference rip-relative for us.
        std::string sym(x.text);
        referenced_.insert(sym);
        return { "[" + mangle(sym) + "]", true, false, false };
    }
    case Op::Ind:
        return { std::string(x.text.substr(1)), false, false, false };
    case Op::Lbl: {
        std::string sym(x.text);
        referenced_.insert(sym);
        return { mangle(sym), false, false, false };
    }
    }
    return {};
}

void MasmSpelling::ins(const std::string &m) {
    const Rule *r = ruleFor(m);
    if (r == nullptr) give_up(m, "an instruction this spelling does not know");
    o_ += "  "; o_ += r->masm; o_ += '\n';
}

void MasmSpelling::ins(const std::string &m, const Op &a) {
    const Rule *r = ruleFor(m);
    if (r == nullptr) give_up(m, "an instruction this spelling does not know");
    Rendered x = render(a);
    std::string t = x.text;
    if (x.isMem && r->width != 0) t = std::string(ptrFor(r->width)) + t;
    o_ += "  "; o_ += r->masm; o_ += ' '; o_ += t; o_ += '\n';
}

void MasmSpelling::ins(const std::string &m, const Op &a, const Op &b) {
    const Rule *r = ruleFor(m);
    if (r == nullptr) give_up(m, "an instruction this spelling does not know");

    // AT&T is source-then-destination; Intel is destination-then-source.
    Rendered src = render(a), dst = render(b);

    // 'movq' between an xmm and a general register is the SSE move and keeps its
    // name; between two general registers it is an ordinary 64-bit 'mov'. Getting
    // this backwards assembles cleanly into the wrong instruction.
    std::string name = r->masm;
    if (m == "movq" && (src.isXmm || dst.isXmm)) name = "movq";

    std::string sTxt = src.text, dTxt = dst.text;
    if (r->width != 0) {
        if (src.isMem) sTxt = std::string(ptrFor(r->width)) + sTxt;
        else if (dst.isMem && m != "movsxd")
            dTxt = std::string(ptrFor(r->width)) + dTxt;
    }
    // A memory destination with an immediate source has no register to take its
    // width from.
    if (r->width == 0 && dst.isMem && src.isImm)
        give_up(m, "a store of an immediate with no width to infer");

    o_ += "  "; o_ += name; o_ += ' '; o_ += dTxt; o_ += ", "; o_ += sTxt; o_ += '\n';
}

void MasmSpelling::defLabel(const std::string &l) {
    if (seg_ == Code) {
        // A bare name here would be a function that bypassed functionBegin, and
        // writing it as a plain label would lose its PROC and its unwind data.
        if (l.compare(0, 3, ".L.") != 0)
            give_up(l, "a label in code that is not an internal one");
        defined_.insert(l);
        o_ += mangle(l); o_ += ":\n";
        return;
    }
    defined_.insert(l);
    flushPending();
    pending_ = mangle(l);
}

void MasmSpelling::functionBegin(const std::string &name, bool exported) {
    defined_.insert(name);
    if (exported) exported_.insert(name);
    flushPending();
    if (seg_ != Code) { o_ += "\n.CODE\n"; seg_ = Code; }
    // FRAME, not a bare PROC: it is what makes ml64 build the .pdata and .xdata
    // entries. x64 Windows has no frame-pointer walk to fall back on, so a
    // function with no entry is a wall that RtlUnwindEx stops at.
    o_ += mangle(name); o_ += " PROC FRAME\n";
}

// The prologue and its description written together, by the one place that
// knows what the prologue is. Unwind data that misdescribes it is worse than
// none, because it is believed.
void MasmSpelling::prologue(int frameSize) {
    o_ += "  push rbp\n";
    o_ += "  .PUSHREG rbp\n";
    o_ += "  mov rbp, rsp\n";
    // The frame register is rsp+0: this happens before the frame is allocated.
    o_ += "  .SETFRAME rbp, 0\n";
    if (frameSize > 0) {
        o_ += "  sub rsp, "; appendNum(o_, frameSize); o_ += '\n';
        o_ += "  .ALLOCSTACK "; appendNum(o_, frameSize); o_ += '\n';
    }
    o_ += "  .ENDPROLOG\n";
}

void MasmSpelling::functionEnd(const std::string &name) {
    o_ += mangle(name); o_ += " ENDP\n\n";
}

void MasmSpelling::globl(const std::string &name) { exported_.insert(name); }

void MasmSpelling::textSection() {
    flushPending();
    if (seg_ != Code) { o_ += "\n.CODE\n"; seg_ = Code; }
}

void MasmSpelling::rodataSection() {
    flushPending();
    if (seg_ != Const) { o_ += "\n.CONST\n"; seg_ = Const; }
}

void MasmSpelling::dataSection() {
    flushPending();
    if (seg_ != Data) { o_ += "\n.DATA\n"; seg_ = Data; }
}

// MASM's fourth segment is '.DATA?', where an item is written '?' and the
// assembler records the size without the bytes.
void MasmSpelling::bssSection() {
    flushPending();
    if (seg_ != Bss) { o_ += "\n.DATA?\n"; seg_ = Bss; }
}

// MASM derives both from the item that defines it, so there is nothing to say.
void MasmSpelling::objectType(const std::string &) {}
void MasmSpelling::objectSize(const std::string &, int) {}

void MasmSpelling::align(int n) {
    flushPending();
    o_ += "  ALIGN "; appendNum(o_, n); o_ += '\n';
}

// GNU counts the bytes; MASM repeats one, and DUP needs a count of at least
// one. In .DATA? the item is '?' rather than 0, or the zeroes reach the object
// file and the segment's whole point is undone.
void MasmSpelling::zero(int n) {
    if (n == 0) return;
    items("DB", { std::to_string(n) +
                  (seg_ == Bss ? " DUP (?)" : " DUP (0)") });
}

void MasmSpelling::dataInt(int size, long long v) {
    const char *dir = size == 1 ? "DB" : size == 2 ? "DW"
                    : size == 4 ? "DD" : "DQ";
    items(dir, { std::to_string(v) });
}

// The name goes through the same mangling every reference does, or a table of
// 'add' and 'sub' points at definitions sitting under '$add' and '$sub'.
void MasmSpelling::dataSym(const std::string &sym, long long off) {
    std::string t = mangle(sym);
    if (off > 0) t += "+" + std::to_string(off);
    else if (off < 0) t += "-" + std::to_string(-off);
    items("DQ", { t });
}

void MasmSpelling::dataBytes(const std::string &bytes) {
    std::vector<std::string> it;
    it.reserve(bytes.size());
    for (char c : bytes)
        it.push_back(std::to_string(
            static_cast<int>(static_cast<unsigned char>(c))));
    items("DB", it);
}

void MasmSpelling::flushPending() {
    if (!pending_.empty()) {
        o_ += pending_; o_ += " LABEL BYTE\n";
        pending_.clear();
    }
}

// MASM gives up on a statement with too many items - "statement too complex" -
// and a string of any length is one .byte in GNU.
void MasmSpelling::items(const char *dir, const std::vector<std::string> &it) {
    const std::size_t kPerLine = 16;
    for (std::size_t i = 0; i < it.size(); i += kPerLine) {
        std::string chunk;
        for (std::size_t j = i; j < it.size() && j < i + kPerLine; j++) {
            if (!chunk.empty()) chunk += ", ";
            chunk += it[j];
        }
        if (i == 0 && !pending_.empty()) {
            o_ += pending_; o_ += ' '; o_ += dir; o_ += ' '; o_ += chunk; o_ += '\n';
            pending_.clear();
        } else {
            o_ += "  "; o_ += dir; o_ += ' '; o_ += chunk; o_ += '\n';
        }
    }
}

// Before anything is emitted, because the mangling consults it mid-stream: a
// call to a reserved name is '$name' when the definition is anywhere in this
// unit, and the unmangled import otherwise.
void MasmSpelling::predefine(const std::vector<std::string> &names) {
    for (const std::string &n : names) defined_.insert(n);
}

// MASM will not resolve a name it has not been told about, where GNU leaves it
// to the linker.
void MasmSpelling::preamble(std::ostream &sink) {
    sink << "; Generated by cc1 for x86_64-windows, in MASM syntax for ml64.\n";
    sink << "; The instruction selection is the same one the Linux backend makes;\n";
    sink << "; only the spelling is Microsoft's. See src/backend/Masm.cpp.\n\n";

    for (const std::string &g : unreserved_)
        sink << "OPTION NOKEYWORD:<" << g << ">\n";
    if (!unreserved_.empty()) sink << "\n";

    for (const std::string &e : exported_)
        sink << "PUBLIC " << mangle(e) << "\n";
    for (const std::string &r : referenced_)
        if (defined_.find(r) == defined_.end())
            sink << "EXTERN " << mangle(r) << ":PROC\n";
    sink << "\n";
}

void MasmSpelling::postamble(std::ostream &sink) {
    // It cannot happen: every data label is followed by its datum or its DUP.
    if (!pending_.empty())
        give_up(pending_, "a data label left dangling at the end of the file");
    sink << "\nEND\n";
}
