#pragma once

#include <string>
#include <vector>

// The compile unit a line table needs before anything will read it.
//
// '.loc' directives are enough for an assembler to build .debug_line, and not
// enough for a debugger: a line program with no DW_TAG_compile_unit pointing
// at it is a table nothing owns. Both assemblers this compiler feeds decline
// to invent that unit - given a .s that already carries .loc, they take the
// producer at its word and write only the line program - so cc1 writes it,
// which is these two sections.
//
// Deliberately the smallest unit that works: a producer, a language, a name, a
// directory, the text's extent, the offset of the line program, and one entry
// per function so a backtrace has names in it. No types and no variables yet -
// those are DW_TAG_variable and a type graph, and they are the next thing.

struct DwarfFunction {
    std::string name;
    std::string begin;   // label at the first instruction
    std::string end;     // label one past the last
    int file;            // 1-based, as .file numbers them
    int line;
    bool external;
};

// How a platform spells the two section headings. Everything else - .byte,
// .short, .long, .quad, .asciz - both assemblers read the same way, which is
// why only this differs.
struct DwarfSections {
    const char *abbrev;
    const char *info;
};

extern const DwarfSections kElfDwarf;
extern const DwarfSections kMachODwarf;

// Appends both sections. 'text' is the extent of the code, and the unit is
// written only when there is at least one function to describe.
void writeDwarf(std::string &out, const DwarfSections &sec,
                const std::string &file, const std::string &compDir,
                const std::vector<DwarfFunction> &fns);
