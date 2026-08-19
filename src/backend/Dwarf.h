#pragma once

#include "../Ast.h"
#include "../Type.h"

#include <string>
#include <vector>

// The debug information, written as assembly directives.
//
// '.loc' directives are enough for an assembler to build .debug_line, and not
// enough for a debugger: a line program with no DW_TAG_compile_unit pointing
// at it is a table nothing owns. Both assemblers this compiler feeds decline
// to invent that unit - given a .s that already carries .loc, they take the
// producer at its word - so cc1 writes it, along with the types and the
// objects a debugger needs before it can print anything.
//
// The bytes are identical on every platform. What differs is three things,
// which is the whole of DwarfSpelling: how a section heading is written, the
// register a frame is measured from, and whether a symbol wears a leading
// underscore.

// One block of a function, as the debug information needs it: who encloses it,
// and the two labels that bound the instructions it produced. The parent is an
// index into the same vector, and [0] is the function's own scope - it needs no
// labels, the subprogram's own pair already bounding it.
struct DwarfBlock {
    int parent;
    std::string begin;
    std::string end;
};

struct DwarfFunction {
    std::string name;
    std::string begin;   // label at the first instruction
    std::string end;     // label one past the last
    int file;            // 1-based, as .file numbers them
    int line;
    bool external;
    const Type *returns;
    const std::vector<Local> *locals;
    // By value rather than by pointer: the labels are worked out while the
    // body is emitted, which is after the function has been recorded, and a
    // vector that grows in the meantime would move what a pointer names.
    std::vector<DwarfBlock> blocks;
};

struct DwarfGlobal {
    std::string name;
    std::string symbol;  // what the assembly calls it, prefix and all
    const Type *type;
    bool external;
};

struct DwarfSpelling {
    const char *abbrev;
    const char *info;
    int frameBaseReg;           // DWARF's number for the frame pointer
    const char *symbolPrefix;   // "_" where the platform adds one
};

extern const DwarfSpelling kElfDwarf;      // x86-64: the frame pointer is 6
extern const DwarfSpelling kMachODwarf;    // arm64: x29 is 29

void writeDwarf(std::string &out, const DwarfSpelling &sp, const Target &target,
                const std::string &file, const std::string &compDir,
                const std::vector<DwarfFunction> &fns,
                const std::vector<DwarfGlobal> &globals);
