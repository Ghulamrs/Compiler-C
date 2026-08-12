// main.cpp - the driver.
//
//   cc1 <file.c> [-o out.s]
//
// Emits assembly only. Assembling and linking stay with gcc, which keeps the
// surface under test to the part being written - and lets every case be
// compiled a second time by gcc and the two results compared, which is what
// tests/run.sh does.
#include "CodeGen.h"
#include "Lexer.h"
#include "Parser.h"
#include "Source.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    std::string in;
    std::string out;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) {
            if (++i == argc) {
                std::fprintf(stderr, "-o needs an argument\n");
                return 1;
            }
            out = argv[i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            std::fprintf(stderr, "unknown option %s\n", argv[i]);
            return 1;
        } else {
            in = argv[i];
        }
    }
    if (in.empty()) {
        std::fprintf(stderr, "usage: cc1 <file.c> [-o out.s]\n");
        return 1;
    }

    Source src = Source::fromFile(in);
    Parser parser(src, Lexer(src).tokenize());
    NodePtr program = parser.parse();

    if (out.empty()) {
        X86_64Linux(std::cout).run(*program);
    } else {
        std::ofstream file(out);
        if (!file) {
            std::fprintf(stderr, "cannot write %s\n", out.c_str());
            return 1;
        }
        X86_64Linux(file).run(*program);
    }
    return 0;
}
