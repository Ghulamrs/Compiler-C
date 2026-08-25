#pragma once

#include <string>
#include <vector>

class Source;

enum class TokenKind { Punct, Num, Str, Ident, Keyword, End };

struct Token {
    TokenKind kind = TokenKind::End;
    long long value = 0;
    bool suffixU = false;
    bool suffixL = false;
    bool suffixLL = false;
    bool isFloat = false;
    bool suffixF = false;
    // Held at the widest floating type the *host* has, because a target's long
    // double may be wider than the host's double.
    long double dvalue = 0;
    // L'x' and L"..." are made of wchar_t rather than char.
    bool wide = false;
    std::string text;
    std::size_t pos = 0;

    // Does this token stand for the given spelling?
    //
    // Only a punctuator, an identifier and a keyword HAVE a spelling. A
    // string's text is its contents, which is a different thing that happens
    // to live in the same field - so the literal "*" answered yes to
    // is("*"), and every parser test for an operator matched it. The visible
    // fault was that printf("%s\n", "*") did not compile: at the start of an
    // expression the parser saw what it took to be a dereference and asked
    // for an operand. "+", "-", "&", "!", "~" and "(" went the same way,
    // those being the rest of what can begin a unary-expression, while "/"
    // and "%" survived by not being able to start one and falling through to
    // the primary parser, which does look at the kind.
    //
    // A number carries no text at all, so it was never at risk; it is named
    // here anyway by being left out, because the rule is "compare a spelling
    // only against something that has one" rather than "avoid strings".
    bool is(const char *s) const {
        return (kind == TokenKind::Punct || kind == TokenKind::Ident ||
                kind == TokenKind::Keyword) && text == s;
    }
};

class Lexer {
public:
    explicit Lexer(const Source &src) : src_(src) {}

    std::vector<Token> tokenize();

private:
    const Source &src_;

    static bool isKeyword(const std::string &word);
};
