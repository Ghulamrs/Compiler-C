#include "Driver.h"

#include "CodeGen.h"
#include "Lexer.h"
#include "Parser.h"
#include "Preprocessor.h"
#include "Source.h"
#include "Type.h"

#include <cstdio>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>

void Driver::usage(char *file) {
    std::fprintf(stderr,
        "usage: %s <file.c> [more.c ...] [-o out.s] [-time]\n"
        "       one .s per input, or -o to name the output of a single input\n"
        "       -time reports how long each phase took\n", file);
}

// a/b/thing.c becomes a/b/thing.s. A source with no .c suffix simply gains .s
// rather than being refused - the suffix is a convention here, not a gate.
std::string Driver::assemblyNameFor(const std::string &source) {
    std::size_t dot = source.rfind('.');
    std::size_t slash = source.find_last_of('/');
    bool hasSuffix = dot != std::string::npos &&
                     (slash == std::string::npos || dot > slash);
    return hasSuffix ? source.substr(0, dot) + ".s" : source + ".s";
}

bool Driver::parseArguments(int argc, char **argv) {
    std::vector<std::string> inputs;
    std::string output;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) {
            if (++i == argc) {
                std::fprintf(stderr, "cc1: -o needs a file name\n");
                return false;
            }
            output = argv[i];
        } else if (std::strcmp(argv[i], "-time") == 0) {
            timing_ = true;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            std::fprintf(stderr, "%s: unknown option %s\n", *argv, argv[i]);
            return false;
        } else {
            inputs.push_back(argv[i]);
        }
    }

    if (inputs.empty()) { usage(argv[0]); return false; }

    // -o names one file. With several inputs there is no one file to name, and
    // silently overwriting the same output with each in turn would be worse
    // than saying so.
    if (!output.empty() && inputs.size() > 1) {
        std::fprintf(stderr,
            "cc1: -o names a single output, but %zu inputs were given\n",
            inputs.size());
        return false;
    }

    for (const std::string &in : inputs) {
        if (!output.empty()) jobs_.push_back(Job{ in, output });
        else if (toStdout_)  jobs_.push_back(Job{ in, "" });
        else                 jobs_.push_back(Job{ in, assemblyNameFor(in) });
    }
    return true;
}

bool Driver::compile(const Job &job) {
    using Clock = std::chrono::steady_clock;
    auto ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    // Everything below is local to this call. No state survives it and none is
    // shared with another job, which is what makes the jobs separable.
    LinuxX86_64 target;
    TypeTable types;

    auto t0 = Clock::now();
    // The preprocessor produces the translation unit the rest of the
    // compiler sees: includes spliced, conditionals resolved, macros gone.
    Source src = Preprocessor(job.input).run();
    auto t1 = Clock::now();

    std::vector<Token> tokens = Lexer(src).tokenize();
    auto t2 = Clock::now();

    Parser parser(src, std::move(tokens), types, target);
    Program program = parser.parse();
    auto t3 = Clock::now();

    bool ok = true;
    if (job.output.empty()) {
        X86_64Linux(std::cout, target).run(program);
    } else {
        std::ofstream file(job.output);
        if (!file) {
            std::fprintf(stderr, "cc1: cannot write %s\n", job.output.c_str());
            return false;
        }
        X86_64Linux(file, target).run(program);
    }
    auto t4 = Clock::now();

    if (timing_) {
        double read = ms(t0, t1), lex = ms(t1, t2), parse = ms(t2, t3), gen = ms(t3, t4);
        double all = ms(t0, t4);
        std::fprintf(stderr,
            "%s: read+pp %.2f  lex %.2f  parse %.2f  codegen %.2f  total %.2f ms"
            "   (front end %.0f%%)\n",
            job.input.c_str(), read, lex, parse, gen, all,
            all > 0 ? 100.0 * (read + lex + parse) / all : 0.0);
    }
    return ok;
}

int Driver::run(int argc, char **argv) {
    // With one input and no -o, write to standard output: that is how the
    // compiler has always behaved and the tests and demo scripts rely on it.
    int inputs = 0;
    bool sawO = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) { sawO = true; i++; }
        else if (argv[i][0] != '-') inputs++;
    }
    toStdout_ = (inputs == 1 && !sawO);

    if (!parseArguments(argc, argv)) return 1;

    for (const Job &job : jobs_)
        if (!compile(job)) return 1;   // a diagnostic already exits(1) on its own
    return 0;
}
