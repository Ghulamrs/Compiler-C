#include "Preprocessor.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace {

bool identStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool identCont(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string trim(const std::string &s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    return s.substr(a, b - a);
}

// The lines of a file, with backslash-newline already spliced. A directive is
// often written across several lines that way, and every stage after this one
// would have to know about it otherwise.
std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> out;
    std::string current;
    std::size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (c == '\n') {
            if (!current.empty() && current.back() == '\\') {
                current.pop_back();      // spliced: the next line continues it
                i++;
                continue;
            }
            out.push_back(current);
            current.clear();
            i++;
            continue;
        }
        if (c == '\r') { i++; continue; }
        current.push_back(c);
        i++;
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

const int kMaxIncludeDepth = 32;

}  // namespace

std::string Preprocessor::directoryOf(const std::string &path) {
    std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

void Preprocessor::fail(int fileIndex, int lineNo, const std::string &line,
                        std::size_t column, const std::string &message) const {
    // Deliberately the same shape as Source::fail: file, line, the text, and a
    // caret. A reader should not be able to tell which stage refused.
    int indent = std::fprintf(stderr, "%s:%d: ", files_[fileIndex].c_str(), lineNo);
    std::fprintf(stderr, "%s\n", line.c_str());
    if (column > line.size()) column = line.size();
    std::fprintf(stderr, "%*s^ %s\n", indent + static_cast<int>(column), "",
                 message.c_str());
    std::exit(1);
}

bool Preprocessor::parentEmitting() const {
    for (std::size_t i = 0; i + 1 < conds_.size(); i++)
        if (!conds_[i].active) return false;
    return true;
}

bool Preprocessor::emitting() const {
    for (const Cond &c : conds_)
        if (!c.active) return false;
    return true;
}

void Preprocessor::emitLine(const std::string &text, int fileIndex, int lineNo) {
    out_ += text;
    out_ += '\n';
    lines_.push_back(Source::Line{ fileIndex, lineNo });
}

std::string Preprocessor::expandName(const std::string &name,
                                     std::vector<std::string> &busy,
                                     int fileIndex, int lineNo) {
    if (name == "__LINE__") return std::to_string(lineNo);
    if (name == "__FILE__") return "\"" + files_[fileIndex] + "\"";

    auto it = macros_.find(name);
    if (it == macros_.end()) return name;

    for (const std::string &b : busy)
        if (b == name) return name;      // already expanding: leave it alone

    busy.push_back(name);
    // The body is expanded in turn, so one macro may be written in terms of
    // another. The busy list is what keeps that from being circular.
    std::string body = it->second.body;
    std::string result;
    std::size_t i = 0;
    while (i < body.size()) {
        if (identStart(body[i])) {
            std::size_t start = i;
            while (i < body.size() && identCont(body[i])) i++;
            result += expandName(body.substr(start, i - start), busy, fileIndex, lineNo);
            continue;
        }
        result += body[i++];
    }
    busy.pop_back();
    return result;
}

// Scans one line the way the lexer would, so that substitution happens only
// where a name is actually a name. A macro called "n" must not rewrite the
// inside of "an error", of 'n', or of a comment.
std::string Preprocessor::expandLine(const std::string &line, int fileIndex,
                                     int lineNo) {
    std::string out;
    std::size_t i = 0;

    while (i < line.size()) {
        if (inBlockComment_) {
            std::size_t end = line.find("*/", i);
            if (end == std::string::npos) { out += line.substr(i); return out; }
            out += line.substr(i, end + 2 - i);
            i = end + 2;
            inBlockComment_ = false;
            continue;
        }

        char c = line[i];

        if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            out += line.substr(i);
            return out;
        }
        if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
            out += "/*";
            i += 2;
            inBlockComment_ = true;
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = c;
            out += c;
            i++;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    out += line[i];
                    out += line[i + 1];
                    i += 2;
                    continue;
                }
                out += line[i];
                if (line[i] == quote) { i++; break; }
                i++;
            }
            continue;
        }
        if (identStart(c)) {
            std::size_t start = i;
            while (i < line.size() && identCont(line[i])) i++;
            std::vector<std::string> busy;
            out += expandName(line.substr(start, i - start), busy, fileIndex, lineNo);
            continue;
        }
        out += c;
        i++;
    }
    return out;
}

std::string Preprocessor::resolveDefined(const std::string &expr, int fileIndex,
                                         int lineNo, const std::string &line) {
    std::string out;
    std::size_t i = 0;
    while (i < expr.size()) {
        if (!identStart(expr[i])) { out += expr[i++]; continue; }
        std::size_t start = i;
        while (i < expr.size() && identCont(expr[i])) i++;
        std::string name = expr.substr(start, i - start);
        if (name != "defined") { out += name; continue; }

        while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i]))) i++;
        bool paren = (i < expr.size() && expr[i] == '(');
        if (paren) {
            i++;
            while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i]))) i++;
        }
        std::size_t nameStart = i;
        while (i < expr.size() && identCont(expr[i])) i++;
        std::string operand = expr.substr(nameStart, i - nameStart);
        if (operand.empty() || !identStart(operand[0]))
            fail(fileIndex, lineNo, line, 0, "'defined' needs a name");
        if (paren) {
            while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i]))) i++;
            if (i >= expr.size() || expr[i] != ')')
                fail(fileIndex, lineNo, line, 0, "'defined(' is missing its ')'");
            i++;
        }
        out += macros_.count(operand) ? "1" : "0";
    }
    return out;
}

// A recursive-descent evaluator over the expanded condition. It is a second,
// smaller copy of the language's expression grammar, and it has to be: this
// runs before there is a token stream, a symbol table or a type, and it must
// answer with none of them. C keeps the two grammars deliberately close, which
// is why the precedence ladder below reads like the parser's.
long Preprocessor::evalCondition(const std::string &raw, int fileIndex, int lineNo,
                                 const std::string &line) {
    std::string expanded = resolveDefined(raw, fileIndex, lineNo, line);
    expanded = expandLine(expanded, fileIndex, lineNo);

    struct E {
        Preprocessor *pp;
        const std::string &s;
        int fileIndex;
        int lineNo;
        const std::string &line;
        std::size_t i = 0;

        void skip() {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        }
        bool take(const char *op) {
            skip();
            std::size_t n = 0;
            while (op[n]) n++;
            if (s.compare(i, n, op) != 0) return false;
            // "&&" must not be read as "&", and "<=" not as "<".
            if (n == 1 && i + 1 < s.size()) {
                char a = s[i], b = s[i + 1];
                if ((a == '&' && b == '&') || (a == '|' && b == '|')) return false;
                if ((a == '<' || a == '>') && (b == '=' || b == a)) return false;
                if ((a == '=' || a == '!') && b == '=') return false;
            }
            i += n;
            return true;
        }
        [[noreturn]] void bad(const std::string &m) { pp->fail(fileIndex, lineNo, line, 0, m); }

        long primary() {
            skip();
            if (i >= s.size()) bad("this condition ends too early");
            if (take("(")) {
                long v = cond();
                skip();
                if (!take(")")) bad("this condition is missing a ')'");
                return v;
            }
            if (s[i] == '\'') {
                // A character constant is an integer here as everywhere else.
                i++;
                long v = 0;
                if (i < s.size() && s[i] == '\\') {
                    i++;
                    char c = i < s.size() ? s[i++] : '0';
                    v = (c == 'n') ? 10 : (c == 't') ? 9 : (c == '0') ? 0 : c;
                } else if (i < s.size()) {
                    v = static_cast<unsigned char>(s[i++]);
                }
                if (i < s.size() && s[i] == '\'') i++;
                return v;
            }
            if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                char *stop = nullptr;
                long v = std::strtol(s.c_str() + i, &stop, 0);
                i = static_cast<std::size_t>(stop - s.c_str());
                while (i < s.size() && (s[i] == 'u' || s[i] == 'U' ||
                                        s[i] == 'l' || s[i] == 'L')) i++;
                return v;
            }
            if (identStart(s[i])) {
                // A name that survived expansion is 0. That is C's rule, and it
                // is what makes "#if NOT_DEFINED" false rather than an error.
                while (i < s.size() && identCont(s[i])) i++;
                return 0;
            }
            bad(std::string("this condition has a stray '") + s[i] + "'");
        }
        long unary() {
            skip();
            if (take("!")) return !unary();
            if (take("~")) return ~unary();
            if (take("-")) return -unary();
            if (take("+")) return unary();
            return primary();
        }
        long mul() {
            long v = unary();
            for (;;) {
                skip();
                if (take("*")) v = v * unary();
                else if (take("/")) { long r = unary(); if (!r) bad("division by zero in this condition"); v = v / r; }
                else if (take("%")) { long r = unary(); if (!r) bad("division by zero in this condition"); v = v % r; }
                else return v;
            }
        }
        long add() {
            long v = mul();
            for (;;) {
                skip();
                if (take("+")) v = v + mul();
                else if (take("-")) v = v - mul();
                else return v;
            }
        }
        long shift() {
            long v = add();
            for (;;) {
                skip();
                if (take("<<")) v = v << add();
                else if (take(">>")) v = v >> add();
                else return v;
            }
        }
        long rel() {
            long v = shift();
            for (;;) {
                skip();
                if (take("<=")) v = v <= shift();
                else if (take(">=")) v = v >= shift();
                else if (take("<")) v = v < shift();
                else if (take(">")) v = v > shift();
                else return v;
            }
        }
        long eq() {
            long v = rel();
            for (;;) {
                skip();
                if (take("==")) v = v == rel();
                else if (take("!=")) v = v != rel();
                else return v;
            }
        }
        long bitAnd() { long v = eq(); for (;;) { skip(); if (take("&")) v = v & eq(); else return v; } }
        long bitXor() { long v = bitAnd(); for (;;) { skip(); if (take("^")) v = v ^ bitAnd(); else return v; } }
        long bitOr()  { long v = bitXor(); for (;;) { skip(); if (take("|")) v = v | bitXor(); else return v; } }
        long land()   { long v = bitOr(); for (;;) { skip(); if (take("&&")) { long r = bitOr(); v = (v && r); } else return v; } }
        long lor()    { long v = land(); for (;;) { skip(); if (take("||")) { long r = land(); v = (v || r); } else return v; } }
        long cond() {
            long c = lor();
            skip();
            if (!take("?")) return c;
            long a = cond();
            skip();
            if (!take(":")) bad("this condition is missing the ':' of a '?:'");
            long b = cond();
            return c ? a : b;
        }
    };

    if (trim(expanded).empty())
        fail(fileIndex, lineNo, line, 0, "this directive needs a condition");

    E e{ this, expanded, fileIndex, lineNo, line };
    long v = e.cond();
    e.skip();
    if (e.i != expanded.size())
        fail(fileIndex, lineNo, line, 0,
             "this condition has something left over: '" + expanded.substr(e.i) + "'");
    return v;
}

void Preprocessor::directive(const std::string &line, int fileIndex, int lineNo) {
    std::size_t i = line.find('#') + 1;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;

    std::size_t nameStart = i;
    while (i < line.size() && identCont(line[i])) i++;
    std::string what = line.substr(nameStart, i - nameStart);
    std::string rest = trim(line.substr(i));

    // The conditionals are read even inside a part being skipped, because the
    // #endif that ends the skipping is one of them.
    if (what == "ifdef" || what == "ifndef") {
        if (rest.empty() || !identStart(rest[0]))
            fail(fileIndex, lineNo, line, nameStart, "'#" + what + "' needs a name");
        bool defined = macros_.count(rest) != 0;
        bool want = (what == "ifdef") ? defined : !defined;
        // Two flags, not one. "active" is whether this arm lets text through;
        // "taken" is whether any arm of this conditional already has, which is
        // the only thing #elif and #else need to know.
        bool on = emitting() && want;
        conds_.push_back(Cond{ on, on, false });
        return;
    }
    if (what == "if") {
        // The condition is evaluated only when it can matter. Inside skipped
        // text it is not read at all - which is what lets a #if that mentions
        // something undefined sit inside a #ifdef that is false.
        bool on = emitting() && evalCondition(rest, fileIndex, lineNo, line) != 0;
        conds_.push_back(Cond{ on, on, false });
        return;
    }
    if (what == "elif") {
        if (conds_.empty())
            fail(fileIndex, lineNo, line, nameStart, "'#elif' with no '#if'");
        if (conds_.back().seenElse)
            fail(fileIndex, lineNo, line, nameStart, "'#elif' after '#else'");
        Cond &c = conds_.back();
        if (c.taken || !parentEmitting()) {
            c.active = false;      // an earlier arm won, or nothing here runs
            return;
        }
        c.active = evalCondition(rest, fileIndex, lineNo, line) != 0;
        if (c.active) c.taken = true;
        return;
    }
    if (what == "else") {
        if (conds_.empty())
            fail(fileIndex, lineNo, line, nameStart, "'#else' with no '#if'");
        if (conds_.back().seenElse)
            fail(fileIndex, lineNo, line, nameStart, "a second '#else'");
        Cond &c = conds_.back();
        c.seenElse = true;
        c.active = !c.taken && parentEmitting();
        c.taken = true;
        return;
    }
    if (what == "endif") {
        if (conds_.empty())
            fail(fileIndex, lineNo, line, nameStart, "'#endif' with no '#if'");
        conds_.pop_back();
        return;
    }
    if (!emitting()) return;      // everything below is skipped while inactive

    if (what == "define") {
        std::size_t j = 0;
        while (j < rest.size() && identCont(rest[j])) j++;
        std::string name = rest.substr(0, j);
        if (name.empty() || !identStart(name[0]))
            fail(fileIndex, lineNo, line, nameStart, "'#define' needs a name");
        // A '(' touching the name makes it function-like, which is a different
        // piece of work: arguments have to be collected across the call, and
        // each one expanded before it is substituted.
        if (j < rest.size() && rest[j] == '(')
            fail(fileIndex, lineNo, line, nameStart,
                 "a function-like macro is not supported yet - '" + name +
                 "' takes parameters, and only object-like macros are expanded");
        macros_[name] = Macro{ trim(rest.substr(j)), fileIndex, lineNo };
        return;
    }
    if (what == "undef") {
        if (rest.empty())
            fail(fileIndex, lineNo, line, nameStart, "'#undef' needs a name");
        macros_.erase(rest);
        return;
    }
    if (what == "include") {
        if (rest.empty() || (rest[0] != '"' && rest[0] != '<'))
            fail(fileIndex, lineNo, line, nameStart,
                 "'#include' needs \"a file\" in quotes");
        if (rest[0] == '<')
            fail(fileIndex, lineNo, line, nameStart,
                 "'#include <...>' is not supported yet - there are no system "
                 "headers here, and \"...\" finds a file beside this one");
        std::size_t close = rest.find('"', 1);
        if (close == std::string::npos)
            fail(fileIndex, lineNo, line, nameStart, "'#include' is missing its '\"'");
        std::string name = rest.substr(1, close - 1);
        std::string path = directoryOf(files_[fileIndex]) + "/" + name;

        if (depth_ >= kMaxIncludeDepth)
            fail(fileIndex, lineNo, line, nameStart,
                 "'#include' is more than " + std::to_string(kMaxIncludeDepth) +
                 " deep - a file probably includes itself");

        std::FILE *fp = std::fopen(path.c_str(), "rb");
        if (!fp)
            fail(fileIndex, lineNo, line, nameStart, "cannot open " + path);
        std::fclose(fp);

        files_.push_back(path);
        int index = static_cast<int>(files_.size()) - 1;
        depth_++;
        processFile(path, index);
        depth_--;
        return;
    }
    if (what == "error") {
        fail(fileIndex, lineNo, line, nameStart,
             rest.empty() ? "#error" : "#error " + rest);
    }
    if (what == "pragma") return;      // ignored, as C allows for unrecognised ones
    if (what.empty()) return;          // "#" on its own is allowed and does nothing

    fail(fileIndex, lineNo, line, nameStart, "unknown directive '#" + what + "'");
}

void Preprocessor::processFile(const std::string &path, int fileIndex) {
    Source file = Source::fromFile(path);
    std::vector<std::string> lines = splitLines(file.text());

    std::size_t condsAtEntry = conds_.size();

    for (std::size_t n = 0; n < lines.size(); n++) {
        const std::string &line = lines[n];
        int lineNo = static_cast<int>(n) + 1;

        // A '#' inside a block comment is not a directive, which is why the
        // comment state is tracked across lines rather than per line.
        std::size_t first = 0;
        while (first < line.size() &&
               std::isspace(static_cast<unsigned char>(line[first]))) first++;
        if (!inBlockComment_ && first < line.size() && line[first] == '#') {
            directive(line, fileIndex, lineNo);
            continue;
        }

        if (!emitting()) {
            // Skipped text is not expanded and not emitted, but comments still
            // have to be tracked through it or the '#endif' inside one would be
            // read as a directive.
            expandLine(line, fileIndex, lineNo);
            continue;
        }
        emitLine(expandLine(line, fileIndex, lineNo), fileIndex, lineNo);
    }

    if (conds_.size() != condsAtEntry)
        fail(fileIndex, static_cast<int>(lines.size()),
             lines.empty() ? std::string() : lines.back(), 0,
             "a conditional in this file was never closed by '#endif'");
}

Source Preprocessor::run() {
    files_.push_back(path_);
    processFile(path_, 0);
    return Source(path_, out_, files_, lines_);
}
