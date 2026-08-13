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
// It now is a loop over threads, and nothing below Driver knows. Be clear about
// what that buys today, because it is not much. Measured on this box: cc1
// compiles all 361 test programs in 0.62 seconds including 361 process starts,
// and the largest single unit takes 1.2 ms and 4 MB. A four-file build spends
// about five milliseconds in here.
//
// The threads are worth having because the structure already implied them, and
// for the arc where this compiler grows a middle end - the front end is 80 to 88
// per cent of the time here only because there is no optimiser, and gcc at -O2
// spends 93 per cent past the parser. They are not worth having for the
// milliseconds, and saying so is cheaper than discovering it later.
//
// Below four inputs the loop stays serial: starting a thread is not free against
// a millisecond of work. -j names the count and -j 1 forces the serial loop,
// which is what to use when a failure needs reproducing.
//
// One thing threads change that is not speed. A diagnostic ends the process, so
// with several in flight the first failure wins and the others are abandoned
// wherever they had got to - which is what already happened serially, except
// that the abandoned jobs may now have written part of an output file. Their .s
// is garbage either way, because the compilation it belonged to did not finish.
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

    // argv[0], kept because every message this driver prints names it - and
    // compile() is past the point where argv is in reach.
    std::string program_;
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
    // 0 chooses, 1 is the serial loop, anything else is taken as written and
    // then capped by the number of jobs and by the cores available.
    unsigned threads_ = 0;

    // Fills jobs_, or explains why it cannot. Returns false to stop.
    bool parseArguments(int argc, char **argv);

    // One translation unit, start to finish. Everything it needs is local to
    // it, which is the property that makes the jobs separable.
    bool compile(const Job &job);

    // Every job, serially or on threads. The choice is here rather than in run
    // so that the rule has one place to live.
    bool runJobs();
    unsigned threadCount() const;

    static std::string assemblyNameFor(const std::string &source);
    static void usage(char *);
};
