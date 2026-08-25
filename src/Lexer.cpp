#include "Lexer.h"
#include "Source.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>

static long long unescape(const std::string &s, std::size_t &i, std::size_t,
                          bool wide = false) {
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

        return wide ? v : (v & 0xff);
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

        bool wide = false;
        if (c == 'L' && i + 1 < s.size() && (s[i + 1] == '\'' || s[i + 1] == '"')) {
            wide = true;
            i++;
            c = s[i];
        }

        if (c == '\'') {
            std::size_t start = i++;
            if (i >= s.size()) src_.fail(start, "unterminated character constant");
            long long v = 0;
            int chars = 0;

            while (i < s.size() && s[i] != '\'') {
                long long one;
                if (s[i] == '\\') { i++; one = unescape(s, i, start, wide); }
                else one = static_cast<unsigned char>(s[i++]);
                v = wide ? one : ((v << 8) | (one & 0xff));
                chars++;
            }
            if (chars == 0) src_.fail(start, "empty character constant");

            if (!wide && chars == 1) v = static_cast<signed char>(v);
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

                if (i < s.size() && (s[i] == 'f' || s[i] == 'F')) { t.suffixF = true; i++; }
                else if (i < s.size() && (s[i] == 'l' || s[i] == 'L')) { t.suffixL = true; i++; }
                out.push_back(std::move(t));
                continue;
            }

            errno = 0;
            t.value = static_cast<long long>(std::strtoull(s.c_str() + i, &stop, 0));
            if (errno == ERANGE)
                src_.fail(i, "this integer constant does not fit any type");
            i = static_cast<std::size_t>(stop - s.c_str());

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
