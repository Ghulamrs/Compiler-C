#include "Dwarf.h"

const DwarfSections kElfDwarf = {
    "  .section .debug_abbrev,\"\",@progbits\n",
    "  .section .debug_info,\"\",@progbits\n",
};

// Mach-O keeps DWARF in its own segment, and 'debug' is the section attribute
// that keeps it out of the loaded image.
const DwarfSections kMachODwarf = {
    "  .section __DWARF,__debug_abbrev,regular,debug\n",
    "  .section __DWARF,__debug_info,regular,debug\n",
};

namespace {

void num(std::string &o, const char *dir, long long v) {
    o += dir;
    o += ' ';
    o += std::to_string(v);
    o += '\n';
}

void text(std::string &o, const char *dir, const std::string &v) {
    o += dir;
    o += ' ';
    o += v;
    o += '\n';
}

// An attribute is a pair, and writing it as one keeps the table readable
// against the DWARF spec's own tables.
void attr(std::string &o, int at, int form) {
    o += "  .byte ";
    o += std::to_string(at);
    o += ", ";
    o += std::to_string(form);
    o += '\n';
}

// Both extents are written as two addresses rather than an address and a
// length, which DWARF 4 allows and Mach-O requires: '.subsections_via_symbols'
// lets the linker move each function on its own, so the distance between two
// of them is not a number the assembler can work out, and asking for it earns
// 'unsupported relocation in __debug_info' from dsymutil.
//
// DWARF 4 constants, spelled out where they are used rather than as an enum
// nobody would read twice.
const int kTagCompileUnit = 0x11, kTagSubprogram = 0x2e;
const int kAtName = 0x03, kAtStmtList = 0x10, kAtLowPc = 0x11, kAtHighPc = 0x12;
const int kAtLanguage = 0x13, kAtCompDir = 0x1b, kAtProducer = 0x25;
const int kAtDeclFile = 0x3a, kAtDeclLine = 0x3b, kAtExternal = 0x3f;
const int kFormAddr = 0x01, kFormData2 = 0x05;
const int kFormString = 0x08, kFormData1 = 0x0b, kFormFlag = 0x0c;
const int kFormSecOffset = 0x17;
const int kLangC89 = 0x01;

}  // namespace

void writeDwarf(std::string &out, const DwarfSections &sec,
                const std::string &file, const std::string &compDir,
                const std::vector<DwarfFunction> &fns) {
    if (fns.empty()) return;

    out += sec.abbrev;
    out += "  .byte 1\n";                      // this table's first abbreviation
    num(out, "  .byte", kTagCompileUnit);
    out += "  .byte 1\n";                      // it has children
    attr(out, kAtProducer, kFormString);
    attr(out, kAtLanguage, kFormData1);
    attr(out, kAtName, kFormString);
    attr(out, kAtCompDir, kFormString);
    attr(out, kAtLowPc, kFormAddr);
    attr(out, kAtHighPc, kFormAddr);
    attr(out, kAtStmtList, kFormSecOffset);
    out += "  .byte 0, 0\n";

    out += "  .byte 2\n";
    num(out, "  .byte", kTagSubprogram);
    out += "  .byte 0\n";                      // no children
    attr(out, kAtName, kFormString);
    attr(out, kAtDeclFile, kFormData1);
    attr(out, kAtDeclLine, kFormData2);
    attr(out, kAtLowPc, kFormAddr);
    attr(out, kAtHighPc, kFormAddr);
    attr(out, kAtExternal, kFormFlag);
    out += "  .byte 0, 0\n";
    out += "  .byte 0\n";                      // no more abbreviations

    // The unit's length counts from just after the length itself, which is
    // what the two labels are for.
    out += sec.info;
    out += "Ldebug.cu.begin:\n";
    out += "  .long Ldebug.cu.end - Ldebug.cu.after.length\n";
    out += "Ldebug.cu.after.length:\n";
    out += "  .short 4\n";                     // DWARF version
    // Both offsets are zero because cc1 writes one unit per object: its
    // abbreviations begin its abbrev section, and its line program begins the
    // line section the assembler builds.
    out += "  .long 0\n";
    out += "  .byte 8\n";                      // pointers are eight bytes

    out += "  .byte 1\n";                      // the compile unit
    text(out, "  .asciz", "\"cc1\"");
    num(out, "  .byte", kLangC89);
    text(out, "  .asciz", "\"" + file + "\"");
    text(out, "  .asciz", "\"" + compDir + "\"");
    text(out, "  .quad", fns.front().begin);
    text(out, "  .quad", fns.back().end);
    out += "  .long 0\n";                      // where the line program starts

    for (std::vector<DwarfFunction>::const_iterator f = fns.begin();
         f != fns.end(); ++f) {
        out += "  .byte 2\n";
        text(out, "  .asciz", "\"" + f->name + "\"");
        num(out, "  .byte", f->file);
        num(out, "  .short", f->line);
        text(out, "  .quad", f->begin);
        text(out, "  .quad", f->end);
        out += "  .byte ";
        out += (f->external ? "1" : "0");
        out += '\n';
    }
    out += "  .byte 0\n";                      // no more children
    out += "Ldebug.cu.end:\n";
}
