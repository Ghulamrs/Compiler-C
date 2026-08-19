#include "Dwarf.h"

#include <map>

const DwarfSpelling kElfDwarf = {
    "  .section .debug_abbrev,\"\",@progbits\n",
    "  .section .debug_info,\"\",@progbits\n",
    6,          // %rbp
    "",
};

// Mach-O keeps DWARF in its own segment, and 'debug' is the section attribute
// that keeps it out of the loaded image.
const DwarfSpelling kMachODwarf = {
    "  .section __DWARF,__debug_abbrev,regular,debug\n",
    "  .section __DWARF,__debug_info,regular,debug\n",
    29,         // x29
    "_",
};

namespace {

// DWARF 4 constants, spelled where they are used rather than as an enum
// nobody would read twice.
const int kTagArray = 0x01, kTagStructure = 0x13, kTagUnion = 0x17;
const int kTagMember = 0x0d, kTagPointer = 0x0f, kTagCompileUnit = 0x11;
const int kTagSubroutine = 0x15, kTagSubrange = 0x21, kTagBase = 0x24;
const int kTagFormalParameter = 0x05, kTagSubprogram = 0x2e, kTagVariable = 0x34;
const int kTagLexicalBlock = 0x0b;
const int kTagUnspecifiedParameters = 0x18;

const int kAtLocation = 0x02, kAtName = 0x03, kAtByteSize = 0x0b;
const int kAtBitSize = 0x0d, kAtStmtList = 0x10, kAtLowPc = 0x11;
const int kAtHighPc = 0x12, kAtLanguage = 0x13, kAtCompDir = 0x1b;
const int kAtProducer = 0x25, kAtUpperBound = 0x2f, kAtDataMemberLocation = 0x38;
const int kAtDeclFile = 0x3a, kAtDeclLine = 0x3b, kAtEncoding = 0x3e;
const int kAtExternal = 0x3f, kAtFrameBase = 0x40, kAtType = 0x49;
const int kAtDataBitOffset = 0x6b;

const int kFormAddr = 0x01, kFormData2 = 0x05, kFormData4 = 0x06;
const int kFormString = 0x08, kFormData1 = 0x0b, kFormFlag = 0x0c;
const int kFormRef4 = 0x13, kFormSecOffset = 0x17, kFormExprLoc = 0x18;

const int kLangC89 = 0x01;
const int kAteFloat = 0x04, kAteSigned = 0x05, kAteSignedChar = 0x06;
const int kAteUnsigned = 0x07, kAteUnsignedChar = 0x08;

const int kOpAddr = 0x03, kOpFbreg = 0x91;

// The abbreviations, by the code that names them in .debug_info. Two shapes
// of a thing that differs only by whether one attribute is there need two
// codes, which is why there are eighteen of these rather than nine.
enum Abbrev {
    kAbCompileUnit = 1,
    kAbSubprogram, kAbSubprogramVoid,
    kAbFormalParameter, kAbVariable, kAbStaticVariable,
    kAbBase, kAbPointer, kAbPointerVoid,
    kAbArray, kAbSubrange, kAbSubrangeOpen,
    kAbStructure, kAbUnion, kAbMember, kAbBitField,
    kAbSubroutine, kAbSubroutineVoid, kAbParamType, kAbUnspecified,
    kAbLexicalBlock
};

void line(std::string &o, const std::string &text) { o += text; o += '\n'; }

void num(std::string &o, const char *dir, long long v) {
    o += dir;
    o += ' ';
    o += std::to_string(v);
    o += '\n';
}

void str(std::string &o, const std::string &v) {
    o += "  .asciz \"";
    o += v;
    o += "\"\n";
}

void bytes(std::string &o, const std::vector<unsigned char> &b) {
    for (std::size_t i = 0; i < b.size(); i++)
        num(o, "  .byte", b[i]);
}

// Signed LEB128, which is what a frame offset is: negative almost always, and
// the sign lives in the last byte's second-highest bit rather than in a
// leading one.
void sleb(std::vector<unsigned char> &out, long long v) {
    bool more = true;
    while (more) {
        unsigned char byte = static_cast<unsigned char>(v & 0x7f);
        v >>= 7;                       // arithmetic, so the sign is kept
        bool signBit = (byte & 0x40) != 0;
        if ((v == 0 && !signBit) || (v == -1 && signBit)) more = false;
        else byte |= 0x80;
        out.push_back(byte);
    }
}

// An attribute is a pair, and writing it as one keeps the table readable
// against the DWARF specification's own tables.
void attr(std::string &o, int at, int form) {
    o += "  .byte ";
    o += std::to_string(at);
    o += ", ";
    o += std::to_string(form);
    o += '\n';
}

void abbrev(std::string &o, int code, int tag, bool children) {
    num(o, "  .byte", code);
    num(o, "  .byte", tag);
    num(o, "  .byte", children ? 1 : 0);
}

void endAbbrev(std::string &o) { o += "  .byte 0, 0\n"; }

// Every type the unit mentions, each written once. Identity is the pointer:
// the parser interns its types, so two objects of the same type point at the
// same Type and get the same DIE.
class Types {
public:
    // Zero means no type at all, which is what DWARF says by leaving the
    // attribute out - a pointer to void, or a function returning void.
    int id(const Type *t) {
        if (t == nullptr || t->isVoid()) return 0;
        std::map<const Type *, int>::iterator it = ids_.find(t);
        if (it != ids_.end()) return it->second;

        int n = static_cast<int>(order_.size()) + 1;
        ids_[t] = n;
        order_.push_back(t);
        // Whatever this one is made of needs a DIE as well. Registering it
        // now rather than while emitting keeps the numbering in one place.
        if (t->isPointer() || t->isArray()) id(t->pointee());
        if (t->isFunction()) {
            id(t->returns());
            for (std::size_t i = 0; i < t->params().size(); i++)
                id(t->params()[i]);
        }
        if (t->isStructOrUnion())
            for (std::size_t i = 0; i < t->members().size(); i++)
                id(t->members()[i].type);
        return n;
    }

    // Not a range-for: registering a struct appends its members' types, so
    // this grows while it is walked and every one of them is emitted.
    std::size_t size() const { return order_.size(); }
    const Type *at(std::size_t i) const { return order_[i]; }

private:
    std::map<const Type *, int> ids_;
    std::vector<const Type *> order_;
};

std::string label(int id) { return "Ldwarf.t" + std::to_string(id); }

// A reference is an offset from the unit's first byte, which the assembler
// works out because both labels are in this section.
void typeRef(std::string &o, int id) {
    line(o, "  .long " + label(id) + " - Ldebug.cu.begin");
}

int encodingFor(const Type *t, const Target &target) {
    if (t->isFloating()) return kAteFloat;
    if (t->kind() == Kind::Char)
        return target.plainCharIsSigned() ? kAteSignedChar : kAteUnsignedChar;
    if (t->kind() == Kind::SChar) return kAteSignedChar;
    if (t->kind() == Kind::UChar) return kAteUnsignedChar;
    return t->isSigned(target) ? kAteSigned : kAteUnsigned;
}

// Where an object is, as the little program DWARF asks for.
std::vector<unsigned char> frameLocation(int offset) {
    std::vector<unsigned char> e;
    e.push_back(kOpFbreg);
    sleb(e, -static_cast<long long>(offset));
    return e;
}

// The length is unsigned LEB128 and is written as one byte, which is the
// whole encoding for anything under 128: the longest expression here is an
// opcode and an eight-byte address.
void exprLoc(std::string &o, const std::vector<unsigned char> &e) {
    num(o, "  .byte", static_cast<long long>(e.size()));
    bytes(o, e);
}

// An object at a fixed address names its symbol, so the expression is written
// out rather than assembled from bytes: the address is a relocation.
void addressLoc(std::string &o, const std::string &symbol) {
    num(o, "  .byte", 9);                 // one opcode and eight bytes
    num(o, "  .byte", kOpAddr);
    line(o, "  .quad " + symbol);
}

void writeAbbrevTable(std::string &o, const DwarfSpelling &sp) {
    o += sp.abbrev;

    abbrev(o, kAbCompileUnit, kTagCompileUnit, true);
    attr(o, kAtProducer, kFormString);
    attr(o, kAtLanguage, kFormData1);
    attr(o, kAtName, kFormString);
    attr(o, kAtCompDir, kFormString);
    attr(o, kAtLowPc, kFormAddr);
    attr(o, kAtHighPc, kFormAddr);
    attr(o, kAtStmtList, kFormSecOffset);
    endAbbrev(o);

    for (int i = 0; i < 2; i++) {
        abbrev(o, i == 0 ? kAbSubprogram : kAbSubprogramVoid, kTagSubprogram, true);
        attr(o, kAtName, kFormString);
        attr(o, kAtDeclFile, kFormData1);
        attr(o, kAtDeclLine, kFormData2);
        attr(o, kAtLowPc, kFormAddr);
        attr(o, kAtHighPc, kFormAddr);
        attr(o, kAtFrameBase, kFormExprLoc);
        attr(o, kAtExternal, kFormFlag);
        if (i == 0) attr(o, kAtType, kFormRef4);
        endAbbrev(o);
    }

    abbrev(o, kAbLexicalBlock, kTagLexicalBlock, true);
    attr(o, kAtLowPc, kFormAddr);
    attr(o, kAtHighPc, kFormAddr);
    endAbbrev(o);

    abbrev(o, kAbFormalParameter, kTagFormalParameter, false);
    attr(o, kAtName, kFormString);
    attr(o, kAtType, kFormRef4);
    attr(o, kAtLocation, kFormExprLoc);
    endAbbrev(o);

    abbrev(o, kAbVariable, kTagVariable, false);
    attr(o, kAtName, kFormString);
    attr(o, kAtType, kFormRef4);
    attr(o, kAtLocation, kFormExprLoc);
    endAbbrev(o);

    abbrev(o, kAbStaticVariable, kTagVariable, false);
    attr(o, kAtName, kFormString);
    attr(o, kAtType, kFormRef4);
    attr(o, kAtLocation, kFormExprLoc);
    attr(o, kAtExternal, kFormFlag);
    endAbbrev(o);

    abbrev(o, kAbBase, kTagBase, false);
    attr(o, kAtName, kFormString);
    attr(o, kAtEncoding, kFormData1);
    attr(o, kAtByteSize, kFormData1);
    endAbbrev(o);

    abbrev(o, kAbPointer, kTagPointer, false);
    attr(o, kAtByteSize, kFormData1);
    attr(o, kAtType, kFormRef4);
    endAbbrev(o);

    abbrev(o, kAbPointerVoid, kTagPointer, false);
    attr(o, kAtByteSize, kFormData1);
    endAbbrev(o);

    abbrev(o, kAbArray, kTagArray, true);
    attr(o, kAtType, kFormRef4);
    endAbbrev(o);

    abbrev(o, kAbSubrange, kTagSubrange, false);
    attr(o, kAtUpperBound, kFormData4);
    endAbbrev(o);

    // An array whose length nobody wrote down - 'extern int a[];' - says so by
    // having no bound rather than by claiming zero.
    abbrev(o, kAbSubrangeOpen, kTagSubrange, false);
    endAbbrev(o);

    abbrev(o, kAbStructure, kTagStructure, true);
    attr(o, kAtName, kFormString);
    attr(o, kAtByteSize, kFormData4);
    endAbbrev(o);

    abbrev(o, kAbUnion, kTagUnion, true);
    attr(o, kAtName, kFormString);
    attr(o, kAtByteSize, kFormData4);
    endAbbrev(o);

    abbrev(o, kAbMember, kTagMember, false);
    attr(o, kAtName, kFormString);
    attr(o, kAtType, kFormRef4);
    attr(o, kAtDataMemberLocation, kFormData4);
    endAbbrev(o);

    // A bit-field is placed by counting bits from the start of the whole
    // object, which is what DW_AT_data_bit_offset means and why it needs no
    // byte offset beside it.
    abbrev(o, kAbBitField, kTagMember, false);
    attr(o, kAtName, kFormString);
    attr(o, kAtType, kFormRef4);
    attr(o, kAtBitSize, kFormData1);
    attr(o, kAtDataBitOffset, kFormData4);
    endAbbrev(o);

    // A function type carries its parameters, so 'int (*)(int)' is not
    // written 'int (*)()' - which in C90 is a different thing entirely, being
    // a function about whose parameters nothing is said.
    for (int i = 0; i < 2; i++) {
        abbrev(o, i == 0 ? kAbSubroutine : kAbSubroutineVoid, kTagSubroutine, true);
        if (i == 0) attr(o, kAtType, kFormRef4);
        endAbbrev(o);
    }

    abbrev(o, kAbParamType, kTagFormalParameter, false);
    attr(o, kAtType, kFormRef4);
    endAbbrev(o);

    // The '...' of a variadic one.
    abbrev(o, kAbUnspecified, kTagUnspecifiedParameters, false);
    endAbbrev(o);

    o += "  .byte 0\n";                   // no more abbreviations
}

void writeType(std::string &o, const Type *t, int id, Types &types,
               const Target &target) {
    line(o, label(id) + ":");

    if (t->isPointer()) {
        int to = types.id(t->pointee());
        num(o, "  .byte", to != 0 ? kAbPointer : kAbPointerVoid);
        num(o, "  .byte", t->size(target));
        if (to != 0) typeRef(o, to);
        return;
    }
    if (t->isArray()) {
        num(o, "  .byte", kAbArray);
        typeRef(o, types.id(t->pointee()));
        if (t->length() >= 1) {
            num(o, "  .byte", kAbSubrange);
            num(o, "  .long", t->length() - 1);
        } else {
            num(o, "  .byte", kAbSubrangeOpen);
        }
        o += "  .byte 0\n";               // no more children
        return;
    }
    if (t->isFunction()) {
        int to = types.id(t->returns());
        num(o, "  .byte", to != 0 ? kAbSubroutine : kAbSubroutineVoid);
        if (to != 0) typeRef(o, to);
        const std::vector<const Type *> &ps = t->params();
        for (std::size_t i = 0; i < ps.size(); i++) {
            num(o, "  .byte", kAbParamType);
            typeRef(o, types.id(ps[i]));
        }
        if (t->isVariadicFn()) num(o, "  .byte", kAbUnspecified);
        o += "  .byte 0\n";              // no more children
        return;
    }
    if (t->isStructOrUnion()) {
        num(o, "  .byte", t->kind() == Kind::Struct ? kAbStructure : kAbUnion);
        str(o, t->tag());
        num(o, "  .long", t->size(target));
        const std::vector<Member> &ms = t->members();
        for (std::size_t i = 0; i < ms.size(); i++) {
            if (ms[i].isBitField()) {
                num(o, "  .byte", kAbBitField);
                str(o, ms[i].name);
                typeRef(o, types.id(ms[i].type));
                num(o, "  .byte", ms[i].width);
                num(o, "  .long", ms[i].offset * 8 + ms[i].bitOffset);
            } else {
                num(o, "  .byte", kAbMember);
                str(o, ms[i].name);
                typeRef(o, types.id(ms[i].type));
                num(o, "  .long", ms[i].offset);
            }
        }
        o += "  .byte 0\n";
        return;
    }

    num(o, "  .byte", kAbBase);
    str(o, t->name());
    num(o, "  .byte", encodingFor(t, target));
    num(o, "  .byte", t->size(target));
}

// One object, wherever in the tree it was declared.
void writeObject(std::string &o, const Local &l, Types &types,
                 const DwarfSpelling &sp) {
    if (!l.staticName.empty()) {
        // A static local outlives its block and lives at an address, so it is
        // described like a global that happens to be written inside one. It
        // still belongs to the block that declared it: the name is in scope
        // there and nowhere else, whatever the storage does.
        num(o, "  .byte", kAbStaticVariable);
        str(o, l.name);
        typeRef(o, types.id(l.type));
        addressLoc(o, sp.symbolPrefix + l.staticName);
        o += "  .byte 0\n";
        return;
    }
    num(o, "  .byte", l.isParam ? kAbFormalParameter : kAbVariable);
    str(o, l.name);
    typeRef(o, types.id(l.type));
    exprLoc(o, frameLocation(l.offset));
}

// Whether a block, or anything nested in it, declares a name. One that
// declares nothing is not written: a debugger would walk past it, and the
// assembler would have to keep two labels alive to bound nothing. Blocks are
// counted from one, [0] being the function's own scope and its own parent -
// starting at zero would recur forever.
bool declaresAnything(const DwarfFunction &f, int scope) {
    if (f.locals != nullptr)
        for (std::size_t i = 0; i < f.locals->size(); i++)
            if ((*f.locals)[i].scope == scope) return true;
    for (std::size_t b = 1; b < f.blocks.size(); b++)
        if (f.blocks[b].parent == scope &&
            declaresAnything(f, static_cast<int>(b)))
            return true;
    return false;
}

// The children of one scope: what it declared, and then the blocks inside it.
// Parameters come first without being sorted, because they are declared first
// and this keeps declaration order - and DWARF requires a subprogram's formal
// parameters to precede its other children.
void writeScope(std::string &o, const DwarfFunction &f, int scope,
                Types &types, const DwarfSpelling &sp) {
    if (f.locals != nullptr)
        for (std::size_t i = 0; i < f.locals->size(); i++)
            if ((*f.locals)[i].scope == scope)
                writeObject(o, (*f.locals)[i], types, sp);

    for (std::size_t b = 1; b < f.blocks.size(); b++) {
        if (f.blocks[b].parent != scope) continue;
        int id = static_cast<int>(b);
        if (!declaresAnything(f, id)) continue;
        // A block the walk never reached has no labels to bound it, and a
        // lexical block without an address range is worse than none. Its
        // names are real either way, so they are written here instead: one
        // level flatter than the source said, and true as far as it goes.
        if (f.blocks[b].begin.empty() || f.blocks[b].end.empty()) {
            writeScope(o, f, id, types, sp);
            continue;
        }
        num(o, "  .byte", kAbLexicalBlock);
        line(o, "  .quad " + f.blocks[b].begin);
        line(o, "  .quad " + f.blocks[b].end);
        writeScope(o, f, id, types, sp);
        o += "  .byte 0\n";           // no more children of this block
    }
}

}  // namespace

void writeDwarf(std::string &out, const DwarfSpelling &sp, const Target &target,
                const std::string &file, const std::string &compDir,
                const std::vector<DwarfFunction> &fns,
                const std::vector<DwarfGlobal> &globals) {
    if (fns.empty()) return;

    writeAbbrevTable(out, sp);

    Types types;

    // The unit's length counts from just after the length itself, which is
    // what the two labels are for.
    out += sp.info;
    line(out, "Ldebug.cu.begin:");
    line(out, "  .long Ldebug.cu.end - Ldebug.cu.after.length");
    line(out, "Ldebug.cu.after.length:");
    out += "  .short 4\n";                // DWARF version
    // Both offsets are zero because cc1 writes one unit per object: its
    // abbreviations begin its abbrev section, and its line program begins the
    // line section the assembler builds.
    out += "  .long 0\n";
    out += "  .byte 8\n";                 // pointers are eight bytes

    num(out, "  .byte", kAbCompileUnit);
    str(out, "cc1");
    num(out, "  .byte", kLangC89);
    str(out, file);
    str(out, compDir);
    line(out, "  .quad " + fns.front().begin);
    line(out, "  .quad " + fns.back().end);
    out += "  .long 0\n";                 // where the line program starts

    // A frame is measured from the frame pointer, which is where this
    // compiler puts every local: the prologue sets it and nothing moves it,
    // so one expression serves the whole function.
    std::vector<unsigned char> frameBase;
    frameBase.push_back(static_cast<unsigned char>(0x70 + sp.frameBaseReg));
    sleb(frameBase, 0);

    for (std::size_t i = 0; i < fns.size(); i++) {
        const DwarfFunction &f = fns[i];
        int returns = types.id(f.returns);
        num(out, "  .byte", returns != 0 ? kAbSubprogram : kAbSubprogramVoid);
        str(out, f.name);
        num(out, "  .byte", f.file);
        num(out, "  .short", f.line);
        line(out, "  .quad " + f.begin);
        line(out, "  .quad " + f.end);
        exprLoc(out, frameBase);
        num(out, "  .byte", f.external ? 1 : 0);
        if (returns != 0) typeRef(out, returns);

        writeScope(out, f, 0, types, sp);
        out += "  .byte 0\n";             // no more children of this function
    }

    for (std::size_t i = 0; i < globals.size(); i++) {
        num(out, "  .byte", kAbStaticVariable);
        str(out, globals[i].name);
        typeRef(out, types.id(globals[i].type));
        addressLoc(out, sp.symbolPrefix + globals[i].symbol);
        num(out, "  .byte", globals[i].external ? 1 : 0);
    }

    // Last, because registering the types above is what decided how many
    // there are - and a reference is an offset, so nothing minds that the
    // types come after the objects that name them.
    for (std::size_t i = 0; i < types.size(); i++)
        writeType(out, types.at(i), static_cast<int>(i) + 1, types, target);

    out += "  .byte 0\n";                 // no more children of the unit
    line(out, "Ldebug.cu.end:");
}
