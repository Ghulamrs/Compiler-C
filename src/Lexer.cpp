#include "Lexer.h"
#include "Source.h"

#include <cctype>
#include <cstdlib>

// One escape, with i standing on the character after the backslash. Anything
// unrecognised is itself, which is what C says for the ones it does not list.
static long unescape(const std::string &s, std::size_t &i, std::size_t) {
    char c = s[i++];
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    default: return static_cast<unsigned char>(c);
    }
}

bool Lexer::isKeyword(const std::string &word) {
    // Checked after an identifier is scanned, never as a prefix: "returned"
    // and "integer" are identifiers, and a prefix test takes the front off both.
    static const char *const kw[] = {
        "int", "return", "void", "if", "else", "while",
        "char", "short", "long", "signed", "unsigned", "sizeof",
        "float", "double",
        "static", "extern", "const", "register",
        "struct", "union", "enum", "typedef",
        "for", "do", "break", "continue",
        "switch", "case", "default", "goto"
    };
    for (const char *k : kw)
        if (word == k) return true;
    return false;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    const std::string &s = src_.text();
    std::size_t i = 0;

    auto identStart = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; };
    auto identCont  = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };

    // Longest match first, always. Scanning "<" before "<=" would split the
    // operator in two and leave a stray "=" that parses as an assignment.
    // Longest match wins, so the three-character operators are tried first: "<<="
    // scanned as "<<" then "=" would be a shift followed by an assignment.
    static const char *const three[] = { "<<=", ">>=" };
    static const char *const two[] = { "==", "!=", "<=", ">=", "<<", ">>", "&&", "||",
                                       "->", "++", "--", "+=", "-=", "*=", "/=", "%=",
                                       "&=", "|=", "^=" };

    while (i < s.size()) {
        char c = s[i];

        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        // A comment is whitespace that happens to be long.
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            std::size_t end = s.find("*/", i + 2);
            if (end == std::string::npos) src_.fail(i, "unterminated comment");
            i = end + 2;
            continue;
        }

        // A character constant has type int in C, not char, so it is simply a
        // number by the time anything else sees it.
        if (c == '\'') {
            std::size_t start = i++;
            if (i >= s.size()) src_.fail(start, "unterminated character constant");
            long v;
            if (s[i] == '\\') { i++; v = unescape(s, i, start); }
            else v = static_cast<unsigned char>(s[i++]);
            if (i >= s.size() || s[i] != '\'')
                src_.fail(start, "unterminated character constant");
            i++;
            Token t;
            t.kind = TokenKind::Num;
            t.value = v;
            t.pos = start;
            out.push_back(std::move(t));
            continue;
        }

        if (c == '"') {
            std::size_t start = i++;
            std::string text;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\n') src_.fail(start, "unterminated string");
                if (s[i] == '\\') { i++; text.push_back(static_cast<char>(unescape(s, i, start))); }
                else text.push_back(s[i++]);
            }
            if (i >= s.size()) src_.fail(start, "unterminated string");
            i++;
            Token t;
            t.kind = TokenKind::Str;
            t.text = std::move(text);
            t.pos = start;
            out.push_back(std::move(t));
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            Token t;
            t.kind = TokenKind::Num;
            t.pos = i;
            char *stop = nullptr;

            // Floating or integer? Decided by scanning ahead, not by parsing
            // twice: strtol would stop happily at the '.' and call 1.5 an int,
            // leaving the '.' to be reported as a stray character.
            std::size_t j = i;
            bool isHex = (s[j] == '0' && j + 1 < s.size() &&
                          (s[j + 1] == 'x' || s[j + 1] == 'X'));
            bool floating = false;
            if (!isHex) {
                while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) j++;
                if (j < s.size() && (s[j] == '.' || s[j] == 'e' || s[j] == 'E'))
                    floating = true;
            }

            if (floating) {
                t.isFloat = true;
                t.dvalue = std::strtod(s.c_str() + i, &stop);
                i = static_cast<std::size_t>(stop - s.c_str());
                // Only the f suffix makes a constant a float; 1.5 is a double.
                if (i < s.size() && (s[i] == 'f' || s[i] == 'F')) { t.suffixF = true; i++; }
                out.push_back(std::move(t));
                continue;
            }

            // Base 0, so 0x1f and 017 are read the way C reads them.
            t.value = std::strtol(s.c_str() + i, &stop, 0);
            i = static_cast<std::size_t>(stop - s.c_str());
            // A suffix is part of the constant and decides its type: 1u is
            // unsigned, 1l is long. Either order, either case.
            for (int n = 0; n < 2 && i < s.size(); n++) {
                if (s[i] == 'u' || s[i] == 'U')      { t.suffixU = true; i++; }
                else if (s[i] == 'l' || s[i] == 'L') { t.suffixL = true; i++; }
                else break;
            }
            out.push_back(std::move(t));
            continue;
        }

        if (identStart(c)) {
            std::size_t start = i;
            while (i < s.size() && identCont(s[i])) i++;
            Token t;
            t.text = s.substr(start, i - start);
            t.kind = isKeyword(t.text) ? TokenKind::Keyword : TokenKind::Ident;
            t.pos = start;
            out.push_back(std::move(t));
            continue;
        }

        bool matched3 = false;
        for (const char *op : three) {
            if (s.compare(i, 3, op) == 0) {
                Token t; t.kind = TokenKind::Punct; t.text = op; t.pos = i;
                out.push_back(std::move(t)); i += 3; matched3 = true; break;
            }
        }
        if (matched3) continue;

        // The ellipsis, before anything shorter can take a bite out of it.
        if (s.compare(i, 3, "...") == 0) {
            Token t;
            t.kind = TokenKind::Punct;
            t.text = "...";
            t.pos = i;
            out.push_back(std::move(t));
            i += 3;
            continue;
        }

        bool matched = false;
        for (const char *op : two) {
            if (s.compare(i, 2, op) == 0) {
                Token t;
                t.kind = TokenKind::Punct;
                t.text = op;
                t.pos = i;
                out.push_back(std::move(t));
                i += 2;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        // Every character the grammar can see. Five times now a grammar rule has
        // been added without adding its punctuation here - the comma, then '&',
        // then the brackets, then the ':' that case labels need, then the '?' -
        // and each time it surfaced as "stray X in program" rather than as
        // anything about the rule. It is the cheapest line in the file to
        // forget and the most confusing one to debug.
        if (std::string("+-*/%()<>={},;!&[].|^~:?").find(c) != std::string::npos) {
            Token t;
            t.kind = TokenKind::Punct;
            t.text.assign(1, c);
            t.pos = i;
            out.push_back(std::move(t));
            i++;
            continue;
        }

        src_.fail(i, std::string("stray '") + c + "' in program");
    }

    Token end;
    end.kind = TokenKind::End;
    end.pos = s.size();
    out.push_back(std::move(end));
    return out;
}
