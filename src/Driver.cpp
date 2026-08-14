#include "Driver.h"

#include "backend/Backend.h"
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
#include <utility>

#ifdef __linux__
#include <sched.h>
#endif

namespace {

const std::size_t kThreadFrom = 4;

}

#ifndef CC1_INCLUDE_DIR
#define CC1_INCLUDE_DIR ""
#endif

void Driver::usage(char *file) {
    std::fprintf(stderr,
        "usage: %s <file.c> [more.c ...] [-o out.s] [-I dir] [-j n] [-arch a] [-time]\n"
        "       one .s per input, or -o to name the output of a single input\n"
        "       -I adds a directory to the ones <...> searches\n"
        "       -j sets how many files are compiled at once; -j 1 is serial\n"
        "       -arch picks the architecture the code is generated for\n"
        "       -time reports how long each phase took\n", file);
}

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
            if (*n == '\0' || (end && *end != '\0') || value < 1) {
                std::fprintf(stderr,
                    "%s: -j needs a positive number of jobs, not '%s'\n", argv[0], n);
                return false;
            }
            threads_ = static_cast<unsigned>(value);
        } else if (std::strncmp(argv[i], "-arch", 5) == 0) {
            const char *name = argv[i][5] == '=' ? argv[i] + 6 : nullptr;
            if (!name) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -arch needs a name - one of %s\n",
                                 argv[0], backendNames().c_str());
                    return false;
                }
                name = argv[i];
            }
            backend_ = findBackend(name);
            if (backend_ == nullptr) {
                std::fprintf(stderr, "%s: unknown architecture '%s' - one of %s\n",
                             argv[0], name, backendNames().c_str());
                return false;
            }
            if (!backend_->emits()) {
                std::fprintf(stderr, "%s: the %s backend is not written yet - it "
                             "knows what its types measure but has no instructions\n",
                             argv[0], backend_->name());
                return false;
            }
        } else if (std::strcmp(argv[i], "-time") == 0) {
            timing_ = true;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            std::fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
            return false;
        } else {
            inputs.push_back(argv[i]);
        }
    }

    if (CC1_INCLUDE_DIR[0] != '\0') searchPath_.push_back(CC1_INCLUDE_DIR);

    if (inputs.empty()) { usage(argv[0]); return false; }

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

    const Target &target = backend_->target();
    TypeTable types;

    auto t0 = Clock::now();
    Source src = Preprocessor(job.input, searchPath_).run();
    auto t1 = Clock::now();

    std::vector<Token> tokens = Lexer(src).tokenize();
    auto t2 = Clock::now();

    Parser parser(src, std::move(tokens), types, target,
                  backend_->abi().structReturnLimit);
    Program program = parser.parse();
    auto t3 = Clock::now();

    bool ok = true;
    if (job.output.empty()) {
        backend_->codegen(std::cout)->run(program);
    } else {
        std::ofstream file(job.output);
        if (!file) {
            std::fprintf(stderr, "%s: cannot write %s\n", program_.c_str(),
                         job.output.c_str());
            return false;
        }
        backend_->codegen(file)->run(program);
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

unsigned Driver::availableCores() {
#ifdef __linux__
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) == 0) {
        std::vector<std::pair<long, long>> cores;
        for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
            if (!CPU_ISSET(cpu, &allowed)) continue;
            std::string base = "/sys/devices/system/cpu/cpu" +
                               std::to_string(cpu) + "/topology/";
            std::ifstream pkgFile(base + "physical_package_id");
            std::ifstream coreFile(base + "core_id");
            long pkg = 0, core = cpu;
            if (!(pkgFile >> pkg) || !(coreFile >> core)) { pkg = 0; core = cpu; }

            std::pair<long, long> id(pkg, core);
            bool seen = false;
            for (const std::pair<long, long> &k : cores)
                if (k == id) { seen = true; break; }
            if (!seen) cores.push_back(id);
        }
        if (!cores.empty()) return static_cast<unsigned>(cores.size());
    }
#endif
    unsigned n = std::thread::hardware_concurrency();
    return n != 0 ? n : 1;
}

unsigned Driver::threadCount() const {
    if (threads_ == 1) return 1;

    unsigned want;
    if (threads_ != 0) {
        want = threads_;
    } else {
        if (jobs_.size() < kThreadFrom) return 1;
        want = availableCores();
    }
    if (want > jobs_.size()) want = static_cast<unsigned>(jobs_.size());
    return want < 1 ? 1 : want;
}

bool Driver::runJobs() {
    unsigned n = threadCount();

    if (timing_)
        std::fprintf(stderr, "%s: %zu jobs on %u thread%s\n", program_.c_str(),
                     jobs_.size(), n, n == 1 ? "" : "s");

    if (n <= 1) {
        for (const Job &job : jobs_)
            if (!compile(job)) return false;
        return true;
    }

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

    int inputs = 0;
    bool sawO = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) { sawO = true; i++; }
        else if (std::strcmp(argv[i], "-I") == 0) i++;
        else if (std::strcmp(argv[i], "-j") == 0) i++;
        else if (argv[i][0] != '-') inputs++;
    }
    toStdout_ = (inputs == 1 && !sawO);

    if (!parseArguments(argc, argv)) return 1;

    return runJobs() ? 0 : 1;
}
