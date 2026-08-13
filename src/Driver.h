// Driver.h - argument handling, and one job per input file.
//
//   cc1 a.c                 -> a.s
//   cc1 a.c b.c c.c         -> a.s b.s c.s
//   cc1 a.c -o out.s        -> out.s
//   cc1 a.c b.c -o out.s    -> refused; -o names one file, not three
//   cc1 -I inc a.c          -> inc searched before the headers cc1 ships
//
// The split from the compiler proper is deliberate and worth stating, because
// it is the whole answer to "can this be done in pieces".
//
// A job is independent of every other job. Each builds its own Source,
// TypeTable, Parser and CodeGen, shares no state, and writes a separate file.
// C is designed this way - a translation unit knows nothing of its neighbours
// until the linker joins them - and honouring that here means the loop over
// jobs could become a loop over processes or threads without touching a line
// of the compiler.
//
// It has not, because it would not pay. Measured on this box: cc1 takes 0.31
// seconds for all 191 test programs and 4 MB of memory for the largest. The
// time in a build of this shape is the assembler, the linker and the running,
// not the front end. Parallelism belongs in whatever calls this - make -j
// already knows how - and the structure here is what lets it.
#pragma once

#include <string>
#include <vector>

class Driver {
public:
    // Returns a process exit status: 0 if every job succeeded.
    int run(int argc, char **argv);

private:
    struct Job {
        std::string input;
        std::string output;
    };

    std::vector<Job> jobs_;
    // Where <...> looks, in order: every -I as written, then the directory of
    // headers this compiler ships. Shipped last, so a -I can shadow a shipped
    // header and never the other way round.
    std::vector<std::string> searchPath_;
    bool toStdout_ = false;
    // -time reports where a compilation actually went. Added to answer a
    // question rather than to decorate: whether the front end dominates, and
    // whether it dominates more as programs grow.
    bool timing_ = false;

    // Fills jobs_, or explains why it cannot. Returns false to stop.
    bool parseArguments(int argc, char **argv);

    // One translation unit, start to finish. Everything it needs is local to
    // it, which is the property that makes the jobs separable.
    bool compile(const Job &job);

    static std::string assemblyNameFor(const std::string &source);
    static void usage(char *);
};
