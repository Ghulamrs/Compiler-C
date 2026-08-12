// Lexer.h - source text to tokens.
#pragma once

#include <string>
#include <vector>

class Source;

enum class TokenKind { Punct, Num, Ident, Keyword, End };

struct Token {
    TokenKind kind = TokenKind::End;
    long value = 0;         // Num only
    bool suffixU = false;   // Num: the literal was written 1u
    bool suffixL = false;   // Num: the literal was written 1l
    std::string text;       // spelling; empty for Num
    std::size_t pos = 0;    // offset into the source, kept for diagnostics

    bool is(const char *s) const { return text == s; }
};

class Lexer {
public:
    explicit Lexer(const Source &src) : src_(src) {}

    // One pass, no lookahead past the character in hand. The whole list is
    // returned rather than pulled token by token: the parser backs up in
    // places, and a vector makes that a subscript instead of a state machine.
    std::vector<Token> tokenize();

private:
    const Source &src_;

    static bool isKeyword(const std::string &word);
};
