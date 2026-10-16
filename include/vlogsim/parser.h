#pragma once

#include <stdexcept>
#include <vector>

#include "vlogsim/ast.h"
#include "vlogsim/token.h"

namespace vlogsim {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

// Recursive-descent parser for the Milestone-1/2 subset:
// module/endmodule, input/output/wire/reg declarations with optional
// [msb:lsb] ranges, `assign` statements, and clocked `always` blocks with
// blocking/non-blocking assignment and if/else — all with C-like
// expression precedence (matches Verilog operator precedence for the
// supported ops).
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Module parse_module();

    // Parses every `module ... endmodule` in the token stream (Milestone 3:
    // multi-module files / hierarchy). Throws ParseError on a duplicate
    // module name.
    Design parse_design();

private:
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& context);
    [[noreturn]] void error(const std::string& msg) const;

    void parse_range(int& msb, int& lsb); // consumes optional [msb:lsb]
    void parse_port_decl(Module& module, PortDirection dir);
    void parse_net_decl(Module& module, bool is_reg);
    void parse_assign(Module& module);
    void parse_always(Module& module);
    void parse_param_decl(Module& module);
    void parse_instance(Module& module);

    StmtPtr parse_statement();
    StmtPtr parse_block();          // begin ... end
    StmtPtr parse_if();
    StmtPtr parse_assignment_stmt(); // ident (= | <=) expr ;

    ExprPtr parse_expr();          // entry point: ternary
    ExprPtr parse_ternary();
    ExprPtr parse_logical_or();
    ExprPtr parse_logical_and();
    ExprPtr parse_bitwise_or();
    ExprPtr parse_bitwise_xor();
    ExprPtr parse_bitwise_and();
    ExprPtr parse_equality();
    ExprPtr parse_relational();
    ExprPtr parse_shift();
    ExprPtr parse_additive();
    ExprPtr parse_multiplicative();
    ExprPtr parse_unary();
    ExprPtr parse_primary();
    ExprPtr parse_concat();

    std::vector<Token> tokens_;
    size_t pos_ = 0;
};

} // namespace vlogsim
