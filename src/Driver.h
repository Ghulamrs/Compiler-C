#pragma once

#include <string>
#include <vector>

class Driver {
public:
    int run(int argc, char **argv);

private:
    struct Job {
        std::string input;
        std::string output;
    };

    std::string program_;
    std::vector<Job> jobs_;
    std::vector<std::string> searchPath_;
    bool toStdout_ = false;
    bool timing_ = false;
    unsigned threads_ = 0;

    bool parseArguments(int argc, char **argv);

    bool compile(const Job &job);

    bool runJobs();
    unsigned threadCount() const;

    static unsigned availableCores();

    static std::string assemblyNameFor(const std::string &source);
    static void usage(char *);
};
