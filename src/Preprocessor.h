#pragma once

#include "Source.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Preprocessor {
public:
    // The predefined macros arrive as text rather than being built here,
    // because what they say is the backend's business and the preprocessor has
    // no notion of a target - it is the one stage that runs before any of that
    // is decided.
    explicit Preprocessor(std::string path, std::vector<std::string> searchPath = {},
                          std::vector<std::pair<std::string, std::string> > predefined = {})
        : path_(std::move(path)), searchPath_(std::move(searchPath)),
          predefined_(std::move(predefined)) {}

    Source run();

private:
    struct Macro {
        std::string body;
        bool functionLike = false;
        bool variadic = false;
        std::vector<std::string> params;
        int file = 0;
        int line = 0;
    };

    struct Cond {
        bool active;
        bool taken;
        bool seenElse;
    };

    std::string path_;
    std::vector<std::string> searchPath_;
    std::vector<std::pair<std::string, std::string> > predefined_;
    std::unordered_map<std::string, Macro> macros_;

    std::string out_;
    std::vector<std::string> files_;
    std::vector<Source::Line> lines_;

    std::vector<Cond> conds_;
    bool inBlockComment_ = false;
    int depth_ = 0;

    // '#line' - C90 6.8.4. What a line *says* it is, against where it actually
    // sits in the file. Both are presentation only: they change what __LINE__,
    // __FILE__ and every diagnostic report, and nothing else. A generator
    // emitting C from another language uses them so that an error points at
    // the file a person wrote rather than the one it produced.
    //
    // physLine_ is the real line the loop is on, which the '#line' handler
    // needs to work out the offset from - it cannot use the reported one,
    // because that is the thing being redefined.
    int physLine_ = 0;
    int lineDelta_ = 0;      // reported line = physical + this
    int fileOverride_ = -1;  // an index into files_, or -1 for the real name

    bool emitting() const;

    void processFile(const std::string &path, int fileIndex);
    void directive(const std::string &line, int fileIndex, int lineNo);
    void emitLine(const std::string &text, int fileIndex, int lineNo);

    std::string expandLine(const std::string &line, int fileIndex, int lineNo);
    std::string expandText(const std::string &s, std::vector<std::string> &busy,
                           int fileIndex, int lineNo, bool trackComments);
    std::vector<std::string> collectArgs(const std::string &s, std::size_t &i,
                                         const std::string &name,
                                         int fileIndex, int lineNo);
    std::string substitute(const Macro &m, const std::vector<std::string> &args,
                           std::vector<std::string> &busy, int fileIndex, int lineNo);
    static std::string stringify(const std::string &arg);
    bool hasOpenCall(const std::string &s) const;

    std::string reportLine_;

    long evalCondition(const std::string &expr, int fileIndex, int lineNo,
                       const std::string &line);
    std::string resolveDefined(const std::string &expr, int fileIndex, int lineNo,
                               const std::string &line);
    bool parentEmitting() const;

    [[noreturn]] void fail(int fileIndex, int lineNo, const std::string &line,
                           std::size_t column, const std::string &message) const;

    static std::string directoryOf(const std::string &path);

    std::string resolveInclude(const std::string &name, bool angled, int fileIndex,
                               std::vector<std::string> &tried) const;
};
