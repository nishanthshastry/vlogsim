// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/lexer.h"
#include "mini_test.h"

using namespace vlogsim;

static void test_keywords_and_identifiers() {
    Lexer lexer("module foo input output wire reg assign endmodule");
    auto tokens = lexer.tokenize();
    CHECK_EQ(tokens.size(), 9u); // 8 tokens + EOF
    CHECK(tokens[0].type == TokenType::KwModule);
    CHECK(tokens[1].type == TokenType::Identifier);
    CHECK_EQ(tokens[1].text, std::string("foo"));
    CHECK(tokens[2].type == TokenType::KwInput);
    CHECK(tokens[3].type == TokenType::KwOutput);
    CHECK(tokens[4].type == TokenType::KwWire);
    CHECK(tokens[5].type == TokenType::KwReg);
    CHECK(tokens[6].type == TokenType::KwAssign);
    CHECK(tokens[7].type == TokenType::KwEndmodule);
    CHECK(tokens[8].type == TokenType::EndOfFile);
}

static void test_operators() {
    Lexer lexer("a == b != c <= d >= e << f >> g && h || ~i");
    auto tokens = lexer.tokenize();
    // ids: a == b != c <= d >= e << f >> g && h || ~ i EOF
    std::vector<TokenType> expected = {
        TokenType::Identifier, TokenType::EqEq, TokenType::Identifier, TokenType::NotEq,
        TokenType::Identifier, TokenType::Le,   TokenType::Identifier, TokenType::Ge,
        TokenType::Identifier, TokenType::Shl,  TokenType::Identifier, TokenType::Shr,
        TokenType::Identifier, TokenType::AmpAmp, TokenType::Identifier, TokenType::PipePipe,
        TokenType::Tilde, TokenType::Identifier, TokenType::EndOfFile,
    };
    CHECK_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size() && i < tokens.size(); ++i) {
        CHECK(tokens[i].type == expected[i]);
    }
}

static void test_sized_number_literals() {
    Lexer lexer("8'hFF 4'b1010 3'd5 42");
    auto tokens = lexer.tokenize();
    CHECK_EQ(tokens.size(), 5u);
    for (int i = 0; i < 4; ++i) CHECK(tokens[i].type == TokenType::Number);
    CHECK_EQ(tokens[0].text, std::string("8'hFF"));
    CHECK_EQ(tokens[1].text, std::string("4'b1010"));
    CHECK_EQ(tokens[2].text, std::string("3'd5"));
    CHECK_EQ(tokens[3].text, std::string("42"));
}

static void test_comments_and_whitespace_are_skipped() {
    Lexer lexer("wire // trailing comment\n /* block comment */ reg");
    auto tokens = lexer.tokenize();
    CHECK_EQ(tokens.size(), 3u);
    CHECK(tokens[0].type == TokenType::KwWire);
    CHECK(tokens[1].type == TokenType::KwReg);
    CHECK(tokens[2].type == TokenType::EndOfFile);
}

static void test_system_task_identifiers() {
    // Milestone 5: identifiers may start with '$' (system tasks/functions).
    Lexer lexer("$display $finish $time");
    auto tokens = lexer.tokenize();
    CHECK_EQ(tokens.size(), 4u);
    for (int i = 0; i < 3; ++i) CHECK(tokens[i].type == TokenType::Identifier);
    CHECK_EQ(tokens[0].text, std::string("$display"));
    CHECK_EQ(tokens[1].text, std::string("$finish"));
    CHECK_EQ(tokens[2].text, std::string("$time"));
}

static void test_string_literal() {
    Lexer lexer(R"("hello world" "line1\nline2" "quote:\"x\"")");
    auto tokens = lexer.tokenize();
    CHECK_EQ(tokens.size(), 4u);
    for (int i = 0; i < 3; ++i) CHECK(tokens[i].type == TokenType::StringLiteral);
    CHECK_EQ(tokens[0].text, std::string("hello world"));
    CHECK_EQ(tokens[1].text, std::string("line1\nline2"));
    CHECK_EQ(tokens[2].text, std::string("quote:\"x\""));
}

int main() {
    test_keywords_and_identifiers();
    test_operators();
    test_sized_number_literals();
    test_comments_and_whitespace_are_skipped();
    test_system_task_identifiers();
    test_string_literal();
    TEST_MAIN_RETURN();
}
