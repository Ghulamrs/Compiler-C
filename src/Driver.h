#pragma once

#include "backend/Backend.h"

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
    const Backend *backend_ = &defaultBackend();
    bool toStdout_ = false;
    bool timing_ = false;
    bool assemblyOnly_ = false;
    bool debug_ = false;
    bool objectOnly_ = false;
    unsigned threads_ = 0;
    std::string linkTo_;
    std::vector<std::string> temporaries_;
    std::vector<std::string> objects_;

    struct MacroEdit {
        std::string name;
        std::string value;
        bool undef;
    };
    std::vector<MacroEdit> macroEdits_;

    bool parseArguments(int argc, char **argv);

    bool compile(const Job &job);

    bool runJobs();
    unsigned threadCount() const;
    bool link();
    bool assembleObjects();
    void removeTemporaries();
    std::vector<std::pair<std::string, std::string> > macrosFor() const;
    void addMacroEdit(const char *text, bool undef);

    static unsigned availableCores();

    static std::string assemblyNameFor(const std::string &source);
    static std::string objectNameFor(const std::string &source);
    static std::string temporaryName(int index);
    static const char *hostCompiler();
    static const char *hostAssembler();
    static const char *hostLinker();
    static void usage(char *);
};
