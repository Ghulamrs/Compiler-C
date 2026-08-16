#include "Lexer.h"
#include "Source.h"

#include <cctype>
#include <cstdlib>

static long long unescape(const std::string &s, std::size_t &i, std::size_t) {
    char c = s[i++];
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case '?': return '?';
    default: break;
    }

    if (c == 'x' || c == 'X') {
        long long v = 0;
        bool any = false;
        while (i < s.size() && std::isxdigit(static_cast<unsigned char>(s[i]))) {
            char d = s[i++];
            int digit = std::isdigit(static_cast<unsigned char>(d))
                      ? d - '0'
                      : (std::tolower(static_cast<unsigned char>(d)) - 'a' + 10);
            v = v * 16 + digit;
            any = true;
        }
        if (!any) return static_cast<unsigned char>(c);
        return v & 0xff;
    }

    if (c >= '0' && c <= '7') {
        long long v = c - '0';
        for (int n = 0; n < 2 && i < s.size() && s[i] >= '0' && s[i] <= '7'; n++)
            v = v * 8 + (s[i++] - '0');
        return v & 0xff;
    }

    return static_cast<unsigned char>(c);
}

bool Lexer::isKeyword(const std::string &word) {
    static const char *const kw[] = {
        "int", "return", "void", "if", "else", "while",
        "char", "short", "long", "signed", "unsigned", "sizeof",
        "float", "double",
        "static", "extern", "const", "register", "volatile",
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

    static const char *const three[] = { "<<=", ">>=" };
    static const char *const two[] = { "==", "!=", "<=", ">=", "<<", ">>", "&&", "||",
                                       "->", "++", "--", "+=", "-=", "*=", "/=", "%=",
                                       "&=", "|=", "^=" };

    while (i < s.size()) {
        char c = s[i];

        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

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

        // An 'L' immediately before a quote is a prefix and not a name: L'x'
        // and L"..." are wide. Tested here, ahead of the identifier path,
        // because once 'L' has been taken as a name the quote is a separate
        // token and the two cannot be put back together.
        bool wide = false;
        if (c == 'L' && i + 1 < s.size() && (s[i + 1] == '\'' || s[i + 1] == '"')) {
            wide = true;
            i++;
            c = s[i];
        }

        if (c == '\'') {
            std::size_t start = i++;
            if (i >= s.size()) src_.fail(start, "unterminated character constant");
            long long v;
            if (s[i] == '\\') { i++; v = unescape(s, i, start); }
            else v = static_cast<unsigned char>(s[i++]);
            // A narrow constant is a char and takes char's signedness, so
            // '\xff' is -1. A wide one is wchar_t and is not narrowed here -
            // the parser gives it the target's type and lets that decide.
            if (!wide) v = static_cast<signed char>(v);
            if (i >= s.size() || s[i] != '\'')
                src_.fail(start, "unterminated character constant");
            i++;
            Token t;
            t.kind = TokenKind::Num;
            t.value = v;
            t.wide = wide;
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
            t.wide = wide;
            t.pos = start;
            out.push_back(std::move(t));
            continue;
        }

        // A '.' begins a number when a digit follows it: .5 is a constant and
        // is not the member operator, and only the next character tells them
        // apart.
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < s.size() &&
             std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
            Token t;
            t.kind = TokenKind::Num;
            t.pos = i;
            char *stop = nullptr;

            std::size_t j = i;
            bool isHex = (s[j] == '0' && j + 1 < s.size() &&
                          (s[j + 1] == 'x' || s[j + 1] == 'X'));
            bool floating = (s[j] == '.');
            if (!isHex && !floating) {
                while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) j++;
                if (j < s.size() && (s[j] == '.' || s[j] == 'e' || s[j] == 'E'))
                    floating = true;
            }

            if (floating) {
                t.isFloat = true;
                t.dvalue = std::strtold(s.c_str() + i, &stop);
                i = static_cast<std::size_t>(stop - s.c_str());
                // 'f' makes it a float and 'l' a long double; C90 6.1.3.1 gives
                // a floating constant exactly one suffix, so this is not the
                // counting loop the integer case below needs.
                if (i < s.size() && (s[i] == 'f' || s[i] == 'F')) { t.suffixF = true; i++; }
                else if (i < s.size() && (s[i] == 'l' || s[i] == 'L')) { t.suffixL = true; i++; }
                out.push_back(std::move(t));
                continue;
            }

            t.value = static_cast<long long>(std::strtoull(s.c_str() + i, &stop, 0));
            i = static_cast<std::size_t>(stop - s.c_str());
            // At most one U and at most two Ls, in either order: u, UL, LLU and
            // ULL are all suffixes and the last of them is three characters,
            // which a loop bounded at two silently left an L behind.
            int us = 0, ls = 0;
            while (i < s.size()) {
                if ((s[i] == 'u' || s[i] == 'U') && us == 0) {
                    t.suffixU = true; us++; i++;
                } else if ((s[i] == 'l' || s[i] == 'L') && ls < 2) {
                    ls++; i++;
                    t.suffixL = true;
                    if (ls == 2) t.suffixLL = true;
                } else {
                    break;
                }
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
