#pragma once

#include <string>
#include <vector>

#include "vlogsim/token.h"

namespace vlogsim {

// Tokenizes a subset of Verilog-2001 source text. Scope matches the
// Milestone-1 combinational grammar (module/port/wire/reg/assign) plus
// keywords reserved for later milestones so the lexer does not need to
// change when the parser grows.
class Lexer {
public:
    explicit Lexer(std::string source);

    // Tokenizes the whole input and returns it, terminated by EndOfFile.
    std::vector<Token> tokenize();

private:
    Token next_token();
    char peek(int offset = 0) const;
    char advance();
    bool match(char expected);
    void skip_whitespace_and_comments();
    Token make_token(TokenType type, const std::string& text);
    Token lex_identifier_or_keyword();
    Token lex_number();

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

} // namespace vlogsim
