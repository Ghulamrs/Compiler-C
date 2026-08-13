#include "Driver.h"

#include "CodeGen.h"
#include "Lexer.h"
#include "Parser.h"
#include "Preprocessor.h"
#include "Source.h"
#include "Type.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace {

// Fewer inputs than this and the serial loop wins: a translation unit here is
// about a millisecond, and starting a thread is not free against that. Four is
// where the line was drawn, and nothing measured argues with it - at this size
// the whole question is worth milliseconds either way.
const std::size_t kThreadFrom = 4;

}  // namespace

// Where this compiler's own headers live, baked in by the Makefile because
// nothing installs this compiler anywhere - it runs from the tree it was built
// in, and that tree knows its own path at build time.
//
// A build that does not set it still works. "g++ src/*.cpp -o cc1" by hand is
// how this gets built when someone is in a hurry; it simply ships no headers,
// and -I is then the only way to reach one.
#ifndef CC1_INCLUDE_DIR
#define CC1_INCLUDE_DIR ""
#endif

void Driver::usage(char *file) {
    std::fprintf(stderr,
        "usage: %s <file.c> [more.c ...] [-o out.s] [-I dir] [-j n] [-time]\n"
        "       one .s per input, or -o to name the output of a single input\n"
        "       -I adds a directory to the ones <...> searches\n"
        "       -j sets how many files are compiled at once; -j 1 is serial\n"
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
                std::fprintf(stderr, "%s: -o needs a file name\n", argv[0]);
                return false;
            }
            output = argv[i];
        } else if (std::strncmp(argv[i], "-I", 2) == 0) {
            // Both spellings, because both are muscle memory: -Iinc attached,
            // and -I inc apart.
            const char *dir = argv[i][2] != '\0' ? argv[i] + 2 : nullptr;
            if (!dir) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -I needs a directory\n", argv[0]);
                    return false;
                }
                dir = argv[i];
            }
            searchPath_.push_back(dir);
        } else if (std::strncmp(argv[i], "-j", 2) == 0) {
            const char *n = argv[i][2] != '\0' ? argv[i] + 2 : nullptr;
            if (!n) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -j needs a number\n", argv[0]);
                    return false;
                }
                n = argv[i];
            }
            char *end = nullptr;
            long value = std::strtol(n, &end, 10);
            // Checked here rather than clamped quietly. "-j -2" is a mistake
            // about what the flag means, and compiling anyway on some number
            // the driver picked would hide it.
            if (*n == '\0' || (end && *end != '\0') || value < 1) {
                std::fprintf(stderr,
                    "%s: -j needs a positive number of jobs, not '%s'\n", argv[0], n);
                return false;
            }
            threads_ = static_cast<unsigned>(value);
        } else if (std::strcmp(argv[i], "-time") == 0) {
            timing_ = true;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            std::fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
            return false;
        } else {
            inputs.push_back(argv[i]);
        }
    }

    // Last, so a -I shadows a shipped header rather than being shadowed by one:
    // a program that carries its own stdio.h wants that one.
    if (CC1_INCLUDE_DIR[0] != '\0') searchPath_.push_back(CC1_INCLUDE_DIR);

    if (inputs.empty()) { usage(argv[0]); return false; }

    // -o names one file. With several inputs there is no one file to name, and
    // silently overwriting the same output with each in turn would be worse
    // than saying so.
    if (!output.empty() && inputs.size() > 1) {
        std::fprintf(stderr,
            "%s: -o names a single output, but %zu inputs were given\n",
            argv[0], inputs.size());
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
    Source src = Preprocessor(job.input, searchPath_).run();
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
            std::fprintf(stderr, "%s: cannot write %s\n", program_.c_str(),
                         job.output.c_str());
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

// How many jobs run at once. One place, because a rule about when to go
// parallel that is written twice is a rule that will disagree with itself.
unsigned Driver::threadCount() const {
    if (threads_ == 1) return 1;
    if (threads_ == 0 && jobs_.size() < kThreadFrom) return 1;

    unsigned want = threads_ != 0 ? threads_
                                  : static_cast<unsigned>(jobs_.size());
    unsigned cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 2;          // it is allowed to not know, and says 0
    if (want > cores) want = cores;
    if (want > jobs_.size()) want = static_cast<unsigned>(jobs_.size());
    return want < 1 ? 1 : want;
}

bool Driver::runJobs() {
    unsigned n = threadCount();
    if (n <= 1) {
        for (const Job &job : jobs_)
            if (!compile(job)) return false;   // a diagnostic exits(1) on its own
        return true;
    }

    // One index the threads share, rather than a slice of the list each. The
    // jobs are not equal - the largest test program costs twenty times the
    // smallest - so fixed shares would leave one thread holding the long ones
    // while the others finished early and waited.
    std::atomic<std::size_t> next{0};
    std::atomic<bool> ok{true};

    std::vector<std::thread> pool;
    pool.reserve(n);
    for (unsigned t = 0; t < n; t++) {
        pool.emplace_back([this, &next, &ok] {
            for (;;) {
                std::size_t i = next.fetch_add(1);
                if (i >= jobs_.size()) return;
                if (!compile(jobs_[i])) { ok.store(false); return; }
            }
        });
    }
    for (std::thread &t : pool) t.join();
    return ok.load();
}

int Driver::run(int argc, char **argv) {
    program_ = argv[0];

    // With one input and no -o, write to standard output: that is how the
    // compiler has always behaved and the tests and demo scripts rely on it.
    int inputs = 0;
    bool sawO = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) { sawO = true; i++; }
        // A separated -I or -j takes the next argument with it. Counting that
        // directory or that number as an input would make "cc1 -I inc a.c" look
        // like two inputs and quietly stop writing to standard output.
        else if (std::strcmp(argv[i], "-I") == 0) i++;
        else if (std::strcmp(argv[i], "-j") == 0) i++;
        else if (argv[i][0] != '-') inputs++;
    }
    toStdout_ = (inputs == 1 && !sawO);

    if (!parseArguments(argc, argv)) return 1;

    return runJobs() ? 0 : 1;
}
