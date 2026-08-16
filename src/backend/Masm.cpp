#include "Masm.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <ostream>
#include <set>
#include <vector>

namespace {

[[noreturn]] void give_up(const std::string &file, const std::string &line,
                         const std::string &why) {
    std::fprintf(stderr, "%s: masm: %s\n  in: %s\n", file.c_str(), why.c_str(),
                 line.c_str());
    std::exit(1);
}

std::string trim(const std::string &s) {
    std::size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    std::size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

// MASM identifiers take letters, digits and '_ $ ? @', but not '.', which
// begins a directive. Every label cc1 invents is dotted - '.L.return.main' -
// so those are rewritten. A name the user wrote has no dot in it and comes
// through unchanged, which it must: 'printf' has to still be 'printf' when the
// linker looks for it.
// Names MASM will not accept as identifiers: the registers, the segment
// registers, and its own operators and directives. C has no such reservations,
// so a program with a global called 'gs' or 'size' is perfectly good C that
// ml64 refuses to parse - 'PUBLIC gs' is a syntax error and '[gs]' is read as a
// segment register.
//
// Such a name is given a '$' in front. Every translation unit here is compiled
// by cc1 and mangles identically, so the two ends of a cross-file reference
// still meet. What this cannot survive is linking against an object from
// another compiler that exports the unmangled name - which is the honest cost
// of the platform's assembler owning these words.
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
        // Instruction mnemonics. MASM reserves every one of them, and C does
        // not - 'add', 'sub', 'mul' and 'in' are all ordinary function names.
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
        // x87 and SSE, where 'fabs' is a name any C program with maths in it
        // may well define for itself.
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

std::string mangle(const std::string &name) {
    if (name.find('.') != std::string::npos) {
        std::string out = "$";
        for (char c : name) out += (c == '.') ? '_' : c;
        return out;
    }
    if (isReservedInMasm(name)) return "$" + name;
    return name;
}

// 'array+8' or 'value' or '42'. Mangles the leading identifier if there is
// one and leaves the offset, and any bare number, exactly as it stands.
std::string mangleDataSymbol(const std::string &payload) {
    if (payload.empty()) return payload;
    char c = payload[0];
    bool startsName = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      c == '_' || c == '.';
    if (!startsName) return payload;            // a number, or already signed

    std::size_t end = 0;
    while (end < payload.size()) {
        char d = payload[end];
        if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') ||
            (d >= '0' && d <= '9') || d == '_' || d == '.') { end++; continue; }
        break;
    }
    return mangle(payload.substr(0, end)) + payload.substr(end);
}

bool isIdentStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.';
}

// Split on commas that are not inside parentheses, so the three parts of
// '(%rax,%r10,4)' stay one operand.
std::vector<std::string> splitOperands(const std::string &s) {
    std::vector<std::string> out;
    int depth = 0;
    std::string cur;
    for (char c : s) {
        if (c == '(') depth++;
        if (c == ')') depth--;
        if (c == ',' && depth == 0) { out.push_back(trim(cur)); cur.clear(); continue; }
        cur += c;
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

struct Operand {
    enum Kind { Reg, Imm, Mem, Sym, Indirect } kind = Reg;
    std::string text;      // already in MASM spelling
    std::string symbol;    // for Sym and for rip-relative Mem
};

// '-4(%rbp)' -> '[rbp-4]', '(%rax,%r10,4)' -> '[rax+r10*4]',
// '.L.str.0(%rip)' -> '[$L_str_0]'.
std::string memoryToMasm(const std::string &disp, const std::string &inside,
                         std::string *ripSymbol) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : inside) {
        if (c == ',') { parts.push_back(trim(cur)); cur.clear(); continue; }
        cur += c;
    }
    parts.push_back(trim(cur));

    std::string base = parts.size() > 0 ? parts[0] : "";
    std::string index = parts.size() > 1 ? parts[1] : "";
    std::string scale = parts.size() > 2 ? parts[2] : "";

    if (base == "%rip") {
        // A rip-relative reference names the symbol; ml64 makes it
        // rip-relative for us, and says so in the object it writes.
        *ripSymbol = mangle(disp);
        return "[" + mangle(disp) + "]";
    }

    std::string out = "[";
    if (!base.empty()) out += base.substr(1);   // drop '%'
    if (!index.empty()) {
        out += "+" + index.substr(1);
        if (!scale.empty() && scale != "1") out += "*" + scale;
    }
    if (!disp.empty() && disp != "0") {
        if (disp[0] == '-') out += disp;
        else                out += "+" + disp;
    }
    out += "]";
    return out;
}

Operand parseOperand(const std::string &raw, const std::string &file,
                     const std::string &line) {
    Operand op;
    std::string s = trim(raw);
    if (s.empty()) give_up(file, line, "an empty operand");

    if (s[0] == '$') {                       // immediate
        op.kind = Operand::Imm;
        op.text = s.substr(1);
        return op;
    }
    if (s[0] == '%') {                       // plain register
        op.kind = Operand::Reg;
        op.text = s.substr(1);
        return op;
    }
    if (s[0] == '*') {                       // call/jmp through a register
        op.kind = Operand::Indirect;
        if (s.size() < 2 || s[1] != '%')
            give_up(file, line, "an indirect operand that is not a register");
        op.text = s.substr(2);
        return op;
    }

    std::size_t open = s.find('(');
    if (open != std::string::npos) {         // memory
        std::size_t close = s.rfind(')');
        if (close == std::string::npos || close < open)
            give_up(file, line, "a memory operand with no closing bracket");
        std::string disp = s.substr(0, open);
        std::string inside = s.substr(open + 1, close - open - 1);
        op.kind = Operand::Mem;
        op.text = memoryToMasm(disp, inside, &op.symbol);
        return op;
    }

    if (isIdentStart(s[0])) {                // a bare symbol: jump or call target
        op.kind = Operand::Sym;
        op.symbol = s;
        op.text = mangle(s);
        return op;
    }
    give_up(file, line, "an operand in a shape this translation has not met");
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

bool isXmm(const Operand &o) {
    return o.kind == Operand::Reg && o.text.compare(0, 3, "xmm") == 0;
}

// The instructions the generator actually selects. Anything outside this set
// stops the compiler rather than being guessed at - see the note in Masm.h.
struct Rule {
    const char *att;     // what the generator writes
    const char *masm;    // what ml64 reads
    int width;           // bytes a memory operand carries, 0 when a register
                         // operand already says
};

const Rule kRules[] = {
    // moves, with the size that GNU keeps in the suffix
    { "movb", "mov", 1 }, { "movw", "mov", 2 },
    { "movl", "mov", 4 }, { "movq", "mov", 8 },
    { "mov",  "mov", 0 }, { "movabs", "mov", 0 },
    { "movslq", "movsxd", 4 },
    { "movsbq", "movsx", 1 }, { "movswq", "movsx", 2 },
    { "movzbq", "movzx", 1 },

    { "lea", "lea", 0 },
    { "push", "push", 0 }, { "pop", "pop", 0 },

    { "add", "add", 0 }, { "sub", "sub", 0 }, { "imul", "imul", 0 },
    { "idiv", "idiv", 0 }, { "div", "div", 0 }, { "neg", "neg", 0 },
    { "and", "and", 0 }, { "or", "or", 0 }, { "xor", "xor", 0 },
    { "shl", "shl", 0 }, { "shr", "shr", 0 }, { "sar", "sar", 0 },
    { "cmp", "cmp", 0 }, { "cdq", "cdq", 0 },

    { "call", "call", 0 }, { "ret", "ret", 0 },
    { "jmp", "jmp", 0 }, { "je", "je", 0 }, { "jne", "jne", 0 },

    { "sete", "sete", 0 }, { "setne", "setne", 0 },
    { "setl", "setl", 0 }, { "setle", "setle", 0 },
    { "setg", "setg", 0 }, { "setge", "setge", 0 },
    { "seta", "seta", 0 }, { "setae", "setae", 0 },
    { "setb", "setb", 0 }, { "setbe", "setbe", 0 },
    { "setp", "setp", 0 }, { "setnp", "setnp", 0 },

    // SSE. Same mnemonics either side; only the operand order turns over.
    { "movsd", "movsd", 8 }, { "movss", "movss", 4 },
    { "movapd", "movapd", 0 }, { "movd", "movd", 0 },
    { "addsd", "addsd", 0 }, { "subsd", "subsd", 0 },
    { "mulsd", "mulsd", 0 }, { "divsd", "divsd", 0 },
    { "addss", "addss", 0 },
    { "ucomisd", "ucomisd", 0 }, { "ucomiss", "ucomiss", 0 },
    { "pxor", "pxor", 0 },
    { "cvtsi2sdq", "cvtsi2sd", 8 }, { "cvttsd2si", "cvttsd2si", 0 },
    { "cvtss2sd", "cvtss2sd", 0 }, { "cvtsd2ss", "cvtsd2ss", 0 },
};

const Rule *ruleFor(const std::string &m) {
    for (const Rule &r : kRules)
        if (m == r.att) return &r;
    return nullptr;
}

}  // namespace

void attToMasm(const std::string &att, std::ostream &out) {
    const std::string file = "cc1";
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < att.size();) {
        std::size_t e = att.find('\n', i);
        if (e == std::string::npos) e = att.size();
        lines.push_back(att.substr(i, e - i));
        i = e + 1;
    }

    // Pass one: what this file defines, and what it therefore has to import.
    // MASM will not resolve a name it has not been told about, where the GNU
    // assembler simply leaves it to the linker - so every call and every data
    // reference to something defined elsewhere needs an EXTERN of its own.
    std::set<std::string> defined, referenced, exported;
    for (const std::string &raw : lines) {
        std::string s = trim(raw);
        if (s.empty() || s[0] == '#') continue;
        if (s.back() == ':' && s.find(' ') == std::string::npos) {
            defined.insert(s.substr(0, s.size() - 1));
            continue;
        }
        if (s.compare(0, 7, ".globl ") == 0) {
            exported.insert(trim(s.substr(7)));
            continue;
        }
        if (s[0] == '.') continue;

        std::size_t sp = s.find(' ');
        if (sp == std::string::npos) continue;
        std::string mnemonic = s.substr(0, sp);
        std::string rest = s.substr(sp + 1);
        if (mnemonic == "call" || mnemonic == "jmp" || mnemonic == "je" ||
            mnemonic == "jne") {
            std::string t = trim(rest);
            if (!t.empty() && isIdentStart(t[0])) referenced.insert(t);
        }
        for (const std::string &o : splitOperands(rest)) {
            std::size_t rip = o.find("(%rip)");
            if (rip != std::string::npos) referenced.insert(trim(o.substr(0, rip)));
        }
    }

    out << "; Generated by cc1 for x86_64-windows, in MASM syntax for ml64.\n";
    out << "; The instruction selection is the same one the Linux backend makes;\n";
    out << "; only the spelling is Microsoft's. See src/backend/Masm.cpp.\n\n";

    for (const std::string &e : exported) out << "PUBLIC " << mangle(e) << "\n";
    for (const std::string &r : referenced)
        if (defined.find(r) == defined.end())
            out << "EXTERN " << mangle(r) << ":PROC\n";
    out << "\n";

    // Pass two. A data label has to be joined to the data that follows it,
    // because MASM defines a datum as 'name DB ...' where GNU writes the label
    // on its own line above it.
    // MASM's fourth segment is '.DATA?', where an item is written with '?'
    // instead of a value and the assembler records the size without the bytes.
    // It is what .bss is called here.
    enum Seg { None, Code, Data, Const, Bss } seg = None;
    std::string openProc;
    std::string pendingLabel;

    auto closeProc = [&]() {
        if (!openProc.empty()) { out << openProc << " ENDP\n\n"; openProc.clear(); }
    };
    auto flushLabel = [&]() {
        if (!pendingLabel.empty()) {
            out << pendingLabel << " LABEL BYTE\n";
            pendingLabel.clear();
        }
    };
    // GNU writes the label on its own line above the data; MASM defines the
    // two together, as 'name DB ...'. So a label waits here for its datum.
    auto emitData = [&](const char *directive, const std::string &payload) {
        // MASM gives up on a statement with too many items in it - "statement
        // too complex" - and a string of any length is one DB in GNU. So the
        // items are dealt out over several statements, which say the same
        // thing. The label goes on the first, and the rest continue it.
        std::vector<std::string> items;
        std::string cur;
        for (char c : payload) {
            if (c == ',') { items.push_back(trim(cur)); cur.clear(); continue; }
            cur += c;
        }
        if (!trim(cur).empty()) items.push_back(trim(cur));

        const std::size_t kPerLine = 16;
        for (std::size_t i = 0; i < items.size(); i += kPerLine) {
            std::string chunk;
            for (std::size_t j = i; j < items.size() && j < i + kPerLine; j++) {
                if (!chunk.empty()) chunk += ", ";
                chunk += items[j];
            }
            if (i == 0 && !pendingLabel.empty()) {
                out << pendingLabel << " " << directive << " " << chunk << "\n";
                pendingLabel.clear();
            } else {
                out << "  " << directive << " " << chunk << "\n";
            }
        }
    };

    for (const std::string &raw : lines) {
        std::string s = trim(raw);
        if (s.empty()) continue;

        if (s.back() == ':' && s.find(' ') == std::string::npos) {   // a label
            std::string name = s.substr(0, s.size() - 1);
            if (seg == Code) {
                if (name.compare(0, 3, ".L.") == 0) {         // internal
                    out << mangle(name) << ":\n";
                } else {                                     // a function
                    closeProc();
                    openProc = mangle(name);
                    out << openProc << " PROC\n";
                }
            } else {
                flushLabel();
                pendingLabel = mangle(name);
            }
            continue;
        }

        if (s[0] == '.') {                                   // a directive
            if (s == ".text") {
                flushLabel();
                if (seg != Code) { out << "\n.CODE\n"; seg = Code; }
            } else if (s == ".data") {
                closeProc(); flushLabel();
                if (seg != Data) { out << "\n.DATA\n"; seg = Data; }
            } else if (s == ".bss") {
                closeProc(); flushLabel();
                if (seg != Bss) { out << "\n.DATA?\n"; seg = Bss; }
            } else if (s.compare(0, 6, ".type ") == 0 ||
                       s.compare(0, 6, ".size ") == 0) {
                // ELF records what a symbol is and how big it is in the symbol
                // table. MASM derives both from the item that defines it, so
                // there is nothing here to say - which is different from not
                // understanding it, and is why these are named rather than
                // left to fall through to give_up.
            } else if (s.compare(0, 8, ".section") == 0) {
                closeProc(); flushLabel();
                if (s.find(".rodata") == std::string::npos)
                    give_up(file, s, "a section this translation does not know");
                if (seg != Const) { out << "\n.CONST\n"; seg = Const; }
            } else if (s.compare(0, 7, ".globl ") == 0) {
                // already emitted as PUBLIC above
            } else if (s.compare(0, 7, ".align ") == 0) {
                flushLabel();
                out << "  ALIGN " << trim(s.substr(7)) << "\n";
            } else if (s.compare(0, 6, ".byte ") == 0) {
                emitData("DB", trim(s.substr(6)));
            } else if (s.compare(0, 6, ".long ") == 0) {
                emitData("DD", trim(s.substr(6)));
            } else if (s.compare(0, 7, ".short ") == 0) {
                emitData("DW", trim(s.substr(7)));
            } else if (s.compare(0, 6, ".word ") == 0) {
                emitData("DW", trim(s.substr(6)));
            } else if (s.compare(0, 6, ".quad ") == 0) {
                // A .quad is the one data directive that can carry a *name*
                // rather than a number - an address constant, as in
                // 'int *p = &g;' or a table of function pointers. The name has
                // to go through the same mangling every other reference does,
                // or a table of 'add' and 'sub' emits two MASM mnemonics and
                // the definitions it means are sitting under '$add' and
                // '$sub'.
                emitData("DQ", mangleDataSymbol(trim(s.substr(6))));
            } else if (s.compare(0, 6, ".zero ") == 0) {
                // GNU counts the bytes; MASM repeats one. 'DUP' is how it says
                // so, and the count has to be at least one for it to be legal.
                //
                // In .DATA? the repeated item is '?' rather than 0: same
                // reservation, but the assembler records only its size, so the
                // zeroes never reach the object file. That is the whole point
                // of the segment, and writing 0 here would quietly undo it.
                std::string n = trim(s.substr(6));
                if (n == "0") { /* nothing to reserve */ }
                else emitData("DB", n + (seg == Bss ? " DUP (?)" : " DUP (0)"));
            } else {
                give_up(file, s, "a directive this translation does not know");
            }
            continue;
        }

        // an instruction
        std::size_t sp = s.find(' ');
        std::string mnemonic = (sp == std::string::npos) ? s : s.substr(0, sp);
        std::string rest = (sp == std::string::npos) ? "" : trim(s.substr(sp + 1));

        const Rule *rule = ruleFor(mnemonic);
        if (rule == nullptr) give_up(file, s, "an instruction this translation "
                                              "does not know");

        std::vector<std::string> raws = splitOperands(rest);
        std::vector<Operand> ops;
        for (const std::string &r : raws) ops.push_back(parseOperand(r, file, s));

        if (ops.empty()) { out << "  " << rule->masm << "\n"; continue; }

        if (ops.size() == 1) {
            std::string t = ops[0].text;
            // 'call *%r11' is a call through the register, which Intel writes
            // as the bare register.
            if (ops[0].kind == Operand::Mem && rule->width != 0)
                t = std::string(ptrFor(rule->width)) + t;
            out << "  " << rule->masm << " " << t << "\n";
            continue;
        }
        if (ops.size() != 2)
            give_up(file, s, "an instruction with more operands than expected");

        // AT&T is source-then-destination; Intel is destination-then-source.
        Operand src = ops[0], dst = ops[1];

        // 'movq' between an xmm and a general register is the SSE move and
        // keeps its name; between two general registers it is an ordinary
        // 64-bit 'mov'. The operands say which, and getting this backwards
        // assembles cleanly into the wrong instruction.
        std::string name = rule->masm;
        if (mnemonic == "movq" && (isXmm(src) || isXmm(dst))) name = "movq";

        std::string sTxt = src.text, dTxt = dst.text;
        int w = rule->width;
        if (w != 0) {
            if (src.kind == Operand::Mem) sTxt = std::string(ptrFor(w)) + sTxt;
            else if (dst.kind == Operand::Mem && mnemonic != "movsxd")
                dTxt = std::string(ptrFor(w)) + dTxt;
        }
        // A memory destination with an immediate source has no register to
        // take its width from, so MASM must be told outright.
        if (w == 0 && dst.kind == Operand::Mem && src.kind == Operand::Imm)
            give_up(file, s, "a store of an immediate with no width to infer");

        out << "  " << name << " " << dTxt << ", " << sTxt << "\n";
    }

    closeProc();
    flushLabel();
    out << "\nEND\n";
}
