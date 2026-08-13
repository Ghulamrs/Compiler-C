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

std::string Preprocessor::stringify(const std::string &arg) {
    // The argument's own text, with the two characters that cannot appear raw
    // inside a string literal escaped. C says the spelling is what is taken,
    // not what it would expand to.
    std::string out = "\"";
    for (char c : trim(arg)) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

std::vector<std::string> Preprocessor::collectArgs(const std::string &s, std::size_t &i,
                                                   const std::string &name,
                                                   int fileIndex, int lineNo) {
    std::vector<std::string> args;
    std::string current;
    int depth = 0;
    i++;                                  // past the '('

    for (; i < s.size(); i++) {
        char c = s[i];
        if (c == '"' || c == '\'') {
            char quote = c;
            current += c;
            i++;
            while (i < s.size()) {
                if (s[i] == '\\' && i + 1 < s.size()) { current += s[i]; current += s[i + 1]; i += 2; continue; }
                current += s[i];
                if (s[i] == quote) break;
                i++;
            }
            continue;
        }
        if (c == '(' || c == '[') { depth++; current += c; continue; }
        if (c == ']') { depth--; current += c; continue; }
        if (c == ')') {
            if (depth == 0) { i++; args.push_back(current); return args; }
            depth--;
            current += c;
            continue;
        }
        // Only a comma outside every bracket separates arguments: "f(g(a, b))"
        // hands one argument to f and two to g.
        if (c == ',' && depth == 0) { args.push_back(current); current.clear(); continue; }
        current += c;
    }
    fail(fileIndex, lineNo, reportLine_, 0,
         "the call to '" + name + "' is missing its ')'");
}

std::string Preprocessor::substitute(const Macro &m, const std::vector<std::string> &args,
                                     std::vector<std::string> &busy,
                                     int fileIndex, int lineNo) {
    auto indexOf = [&m](const std::string &name) -> int {
        for (std::size_t k = 0; k < m.params.size(); k++)
            if (m.params[k] == name) return static_cast<int>(k);
        return -1;
    };

    const std::string &body = m.body;
    std::string out;
    std::size_t i = 0;

    while (i < body.size()) {
        // '##' pastes what is on either side of it, and both sides are taken as
        // written: expanding them first would put a space between the halves of
        // a name that is supposed to become one.
        if (body.compare(i, 2, "##") == 0) {
            while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back())))
                out.pop_back();
            i += 2;
            while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) i++;
            if (i < body.size() && identStart(body[i])) {
                std::size_t start = i;
                while (i < body.size() && identCont(body[i])) i++;
                std::string name = body.substr(start, i - start);
                int p = indexOf(name);
                out += (p >= 0) ? trim(args[static_cast<std::size_t>(p)]) : name;
            } else if (i < body.size()) {
                out += body[i++];
            }
            continue;
        }
        // '#name' is the argument's spelling as a string literal.
        if (body[i] == '#') {
            std::size_t j = i + 1;
            while (j < body.size() && std::isspace(static_cast<unsigned char>(body[j]))) j++;
            std::size_t start = j;
            while (j < body.size() && identCont(body[j])) j++;
            std::string name = body.substr(start, j - start);
            int p = indexOf(name);
            if (p < 0)
                fail(fileIndex, lineNo, reportLine_, 0,
                     "'#' needs a parameter of this macro after it, and '" + name +
                     "' is not one");
            out += stringify(args[static_cast<std::size_t>(p)]);
            i = j;
            continue;
        }
        if (body[i] == '"' || body[i] == '\'') {
            char quote = body[i];
            out += body[i++];
            while (i < body.size()) {
                if (body[i] == '\\' && i + 1 < body.size()) { out += body[i]; out += body[i + 1]; i += 2; continue; }
                out += body[i];
                if (body[i] == quote) { i++; break; }
                i++;
            }
            continue;
        }
        if (identStart(body[i])) {
            std::size_t start = i;
            while (i < body.size() && identCont(body[i])) i++;
            std::string name = body.substr(start, i - start);
            int p = indexOf(name);
            if (p < 0) { out += name; continue; }
            // Not pasted and not stringified, so the argument is expanded
            // before it goes in - which is what makes "SQUARE(N)" work when N is
            // itself a macro. The parentheses stay the caller's problem, as
            // they are in C.
            std::vector<std::string> argBusy = busy;
            out += expandText(args[static_cast<std::size_t>(p)], argBusy,
                              fileIndex, lineNo, false);
            continue;
        }
        out += body[i++];
    }
    return out;
}

bool Preprocessor::hasOpenCall(const std::string &s) const {
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '"' || s[i] == '\'') {
            char quote = s[i++];
            while (i < s.size()) {
                if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
                if (s[i] == quote) { i++; break; }
                i++;
            }
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') return false;
        if (!identStart(s[i])) { i++; continue; }
        std::size_t start = i;
        while (i < s.size() && identCont(s[i])) i++;
        auto it = macros_.find(s.substr(start, i - start));
        if (it == macros_.end() || !it->second.functionLike) continue;

        std::size_t j = i;
        while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) j++;
        if (j >= s.size()) return true;          // the '(' may be on the next line
        if (s[j] != '(') continue;
        int depth = 0;
        for (; j < s.size(); j++) {
            if (s[j] == '(') depth++;
            else if (s[j] == ')') { depth--; if (depth == 0) break; }
        }
        if (depth != 0) return true;             // arguments continue below
        i = j;
    }
    return false;
}

std::string Preprocessor::expandText(const std::string &s, std::vector<std::string> &busy,
                                     int fileIndex, int lineNo, bool trackComments) {
    std::string out;
    std::size_t i = 0;

    while (i < s.size()) {
        if (trackComments && inBlockComment_) {
            std::size_t end = s.find("*/", i);
            if (end == std::string::npos) { out += s.substr(i); return out; }
            out += s.substr(i, end + 2 - i);
            i = end + 2;
            inBlockComment_ = false;
            continue;
        }

        char c = s[i];

        if (trackComments && c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            out += s.substr(i);
            return out;
        }
        if (trackComments && c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            out += "/*";
            i += 2;
            inBlockComment_ = true;
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = c;
            out += c;
            i++;
            while (i < s.size()) {
                if (s[i] == '\\' && i + 1 < s.size()) { out += s[i]; out += s[i + 1]; i += 2; continue; }
                out += s[i];
                if (s[i] == quote) { i++; break; }
                i++;
            }
            continue;
        }
        if (!identStart(c)) { out += c; i++; continue; }

        std::size_t start = i;
        while (i < s.size() && identCont(s[i])) i++;
        std::string name = s.substr(start, i - start);

        if (name == "__LINE__") { out += std::to_string(lineNo); continue; }
        if (name == "__FILE__") { out += "\"" + files_[fileIndex] + "\""; continue; }

        auto it = macros_.find(name);
        if (it == macros_.end()) { out += name; continue; }

        bool recursing = false;
        for (const std::string &b : busy)
            if (b == name) { recursing = true; break; }
        if (recursing) { out += name; continue; }

        if (!it->second.functionLike) {
            busy.push_back(name);
            out += expandText(it->second.body, busy, fileIndex, lineNo, false);
            busy.pop_back();
            continue;
        }

        // A function-like macro is only invoked when a '(' follows. "MAX" on its
        // own is an ordinary identifier, which is C's rule and is what lets a
        // macro share a name with something that is not called.
        std::size_t j = i;
        while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) j++;
        if (j >= s.size() || s[j] != '(') { out += name; continue; }

        i = j;
        std::vector<std::string> args = collectArgs(s, i, name, fileIndex, lineNo);
        // "F()" with one parameter passes one empty argument; with none it
        // passes none. The difference is invisible in the text and matters here.
        if (args.size() == 1 && trim(args[0]).empty() && it->second.params.empty())
            args.clear();
        if (args.size() != it->second.params.size())
            fail(fileIndex, lineNo, reportLine_, 0,
                 "'" + name + "' takes " + std::to_string(it->second.params.size()) +
                 " argument(s), given " + std::to_string(args.size()));

        // The order of these two lines is C's rule, not a detail. Arguments are
        // expanded in the caller's context - before this macro is marked busy -
        // so a call to the same macro inside an argument still expands:
        // "MAX(MAX(1, 9), 2)" needs the inner one. Only the replacement list is
        // rescanned with the name blocked, which is what stops the recursion.
        std::string replaced = substitute(it->second, args, busy, fileIndex, lineNo);
        busy.push_back(name);
        out += expandText(replaced, busy, fileIndex, lineNo, false);
        busy.pop_back();
    }
    return out;
}

// Scans one line the way the lexer would, so that substitution happens only
// where a name is actually a name. A macro called "n" must not rewrite the
// inside of "an error", of 'n', or of a comment.
std::string Preprocessor::expandLine(const std::string &line, int fileIndex,
                                     int lineNo) {
    std::vector<std::string> busy;
    reportLine_ = line;
    return expandText(line, busy, fileIndex, lineNo, true);
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
        // A '(' *touching* the name makes it function-like. With a space before
        // it, the parenthesis is the first thing in the body instead - so
        // "#define A (x)" and "#define A(x)" are different declarations, and
        // the only thing telling them apart is that space.
        if (j < rest.size() && rest[j] == '(') {
            std::vector<std::string> params;
            std::size_t k = j + 1;
            for (;;) {
                while (k < rest.size() && std::isspace(static_cast<unsigned char>(rest[k]))) k++;
                if (k < rest.size() && rest[k] == ')') { k++; break; }
                if (rest.compare(k, 3, "...") == 0)
                    fail(fileIndex, lineNo, line, nameStart,
                         "a variadic macro is not supported yet - '...' and "
                         "__VA_ARGS__ are C99, and this is an ANSI C compiler");
                std::size_t start = k;
                while (k < rest.size() && identCont(rest[k])) k++;
                std::string param = rest.substr(start, k - start);
                if (param.empty() || !identStart(param[0]))
                    fail(fileIndex, lineNo, line, nameStart,
                         "'" + name + "' has a parameter list this is not a name in");
                for (const std::string &p : params)
                    if (p == param)
                        fail(fileIndex, lineNo, line, nameStart,
                             "'" + name + "' names the parameter '" + param + "' twice");
                params.push_back(param);
                while (k < rest.size() && std::isspace(static_cast<unsigned char>(rest[k]))) k++;
                if (k < rest.size() && rest[k] == ',') {
                    k++;
                    // A comma promises another parameter. "P(a, )" is a
                    // parameter list with a hole in it.
                    std::size_t look = k;
                    while (look < rest.size() &&
                           std::isspace(static_cast<unsigned char>(rest[look]))) look++;
                    if (look < rest.size() && rest[look] == ')')
                        fail(fileIndex, lineNo, line, nameStart,
                             "'" + name + "' has a ',' with no parameter after it");
                    continue;
                }
                if (k < rest.size() && rest[k] == ')') { k++; break; }
                fail(fileIndex, lineNo, line, nameStart,
                     "'" + name + "' has a parameter list that is missing its ')'");
            }
            Macro m;
            m.body = trim(rest.substr(k));
            m.functionLike = true;
            m.params = params;

            // '#' must be followed by one of this macro's parameters, and that
            // is knowable now. Leaving it until the macro is used would mean a
            // definition nobody calls is never checked at all - and would
            // report the mistake at the call rather than where it was written.
            for (std::size_t q = 0; q < m.body.size(); q++) {
                if (m.body[q] == '"' || m.body[q] == '\'') {
                    char quote = m.body[q++];
                    while (q < m.body.size() && m.body[q] != quote) {
                        if (m.body[q] == '\\') q++;
                        q++;
                    }
                    continue;
                }
                if (m.body[q] != '#') continue;
                if (q + 1 < m.body.size() && m.body[q + 1] == '#') { q++; continue; }
                std::size_t r = q + 1;
                while (r < m.body.size() &&
                       std::isspace(static_cast<unsigned char>(m.body[r]))) r++;
                std::size_t startName = r;
                while (r < m.body.size() && identCont(m.body[r])) r++;
                std::string after = m.body.substr(startName, r - startName);
                bool isParam = false;
                for (const std::string &pn : params)
                    if (pn == after) { isParam = true; break; }
                if (!isParam)
                    fail(fileIndex, lineNo, line, nameStart,
                         after.empty()
                             ? "'#' needs a parameter of '" + name + "' after it"
                             : "'#' needs a parameter of '" + name + "' after it, and '" +
                               after + "' is not one");
                q = r - 1;
            }
            m.file = fileIndex;
            m.line = lineNo;
            macros_[name] = m;
            return;
        }
        Macro m;
        m.body = trim(rest.substr(j));
        m.file = fileIndex;
        m.line = lineNo;
        macros_[name] = m;
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

        // A call to a function-like macro may be written across several lines,
        // and the line it starts on is not enough to expand it. The rest are
        // pulled in here, joined by a space, and the whole thing is emitted as
        // one line - so the map reports the line the call started on, which is
        // where a reader would look for it.
        std::string logical = line;
        while (hasOpenCall(logical) && n + 1 < lines.size()) {
            n++;
            logical += " ";
            logical += lines[n];
        }
        emitLine(expandLine(logical, fileIndex, lineNo), fileIndex, lineNo);
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
