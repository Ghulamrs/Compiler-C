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

struct DwarfFunction {
    std::string name;
    std::string begin;   // label at the first instruction
    std::string end;     // label one past the last
    int file;            // 1-based, as .file numbers them
    int line;
    bool external;
    const Type *returns;
    const std::vector<Local> *locals;
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
