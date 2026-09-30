#include "vlogsim/parser.h"

#include <cctype>

namespace vlogsim {

namespace {

// Parses a Verilog numeric literal such as "42", "8'hFF", "4'b1010",
// "3'd5", "'o17". Returns {width, value}; width is -1 when unsized.
std::pair<int, uint64_t> parse_number_literal(const std::string& text) {
    size_t tick = text.find('\'');
    if (tick == std::string::npos) {
        // Plain decimal, unsized.
        uint64_t value = text.empty() ? 0 : std::stoull(text);
        return {-1, value};
    }

    std::string size_part = text.substr(0, tick);
    int width = size_part.empty() ? -1 : std::stoi(size_part);

    size_t i = tick + 1;
    if (i < text.size() && (text[i] == 's' || text[i] == 'S')) i++; // signed marker
    char base = 'd';
    if (i < text.size()) {
        base = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
        i++;
    }

    std::string digits;
    for (; i < text.size(); ++i) {
        if (text[i] != '_') digits.push_back(text[i]);
    }

    int radix = 10;
    switch (base) {
        case 'b': radix = 2; break;
        case 'o': radix = 8; break;
        case 'd': radix = 10; break;
        case 'h': radix = 16; break;
        default: radix = 10; break;
    }

    uint64_t value = 0;
    if (!digits.empty()) {
        // x/z digits are treated as 0 in this two-state Milestone-1 evaluator;
        // four-state support arrives in the Milestone-4 upgrade.
        std::string clean;
        clean.reserve(digits.size());
        for (char c : digits) {
            if (c == 'x' || c == 'X' || c == 'z' || c == 'Z') clean.push_back('0');
            else clean.push_back(c);
        }
        value = std::stoull(clean, nullptr, radix);
    }
    return {width, value};
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    if (idx >= tokens_.size()) return tokens_.back(); // EOF
    return tokens_[idx];
}

const Token& Parser::advance() {
    const Token& t = peek();
    if (pos_ < tokens_.size() - 1) pos_++;
    return t;
}

bool Parser::check(TokenType type) const { return peek().type == type; }

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

const Token& Parser::expect(TokenType type, const std::string& context) {
    if (!check(type)) {
        error("expected " + std::string(token_type_name(type)) + " " + context + " but found '" +
              peek().text + "'");
    }
    return advance();
}

void Parser::error(const std::string& msg) const {
    const Token& t = peek();
    throw ParseError("line " + std::to_string(t.line) + ": " + msg);
}

void Parser::parse_range(int& msb, int& lsb) {
    msb = 0;
    lsb = 0;
    if (!match(TokenType::LBracket)) return;
    Token msb_tok = expect(TokenType::Number, "in range");
    msb = static_cast<int>(parse_number_literal(msb_tok.text).second);
    expect(TokenType::Colon, "in range");
    Token lsb_tok = expect(TokenType::Number, "in range");
    lsb = static_cast<int>(parse_number_literal(lsb_tok.text).second);
    expect(TokenType::RBracket, "closing range");
}

void Parser::parse_port_decl(Module& module, PortDirection dir) {
    int msb, lsb;
    parse_range(msb, lsb);
    for (;;) {
        Token name = expect(TokenType::Identifier, "port name");
        Port p;
        p.name = name.text;
        p.direction = dir;
        p.msb = msb;
        p.lsb = lsb;
        module.ports.push_back(p);
        if (!match(TokenType::Comma)) break;
    }
    expect(TokenType::Semicolon, "after port declaration");
}

void Parser::parse_net_decl(Module& module, bool is_reg) {
    int msb, lsb;
    parse_range(msb, lsb);
    for (;;) {
        Token name = expect(TokenType::Identifier, "net name");
        NetDecl decl{name.text, msb, lsb};
        if (is_reg) module.regs.push_back(decl);
        else module.wires.push_back(decl);
        if (!match(TokenType::Comma)) break;
    }
    expect(TokenType::Semicolon, "after net declaration");
}

void Parser::parse_assign(Module& module) {
    Token target = expect(TokenType::Identifier, "assign target");
    expect(TokenType::Assign, "in continuous assignment");
    ExprPtr rhs = parse_expr();
    expect(TokenType::Semicolon, "after assign statement");
    module.assigns.push_back(ContinuousAssign{target.text, std::move(rhs)});
}

Module Parser::parse_module() {
    Module module;
    expect(TokenType::KwModule, "at start of file");
    Token name = expect(TokenType::Identifier, "module name");
    module.name = name.text;

    // Optional ANSI-style port list in the module header:
    // module foo(input [3:0] a, output y);
    // Each entry must carry an explicit input/output keyword (no implicit
    // direction inheritance across commas in this subset).
    if (match(TokenType::LParen)) {
        if (!check(TokenType::RParen)) {
            for (;;) {
                if (check(TokenType::KwInput) || check(TokenType::KwOutput)) {
                    // ANSI style: the direction (and optional range) is given
                    // right here, e.g. module foo(input [3:0] a, output y);
                    PortDirection dir;
                    if (match(TokenType::KwInput)) {
                        dir = PortDirection::Input;
                    } else {
                        advance(); // consume 'output'
                        dir = PortDirection::Output;
                    }
                    int msb, lsb;
                    parse_range(msb, lsb);
                    Token name = expect(TokenType::Identifier, "port name");
                    Port p;
                    p.name = name.text;
                    p.direction = dir;
                    p.msb = msb;
                    p.lsb = lsb;
                    module.ports.push_back(p);
                } else {
                    // Non-ANSI style: header only lists names; direction and
                    // width are declared separately in the module body via
                    // input/output statements.
                    expect(TokenType::Identifier, "port name in module header");
                }
                if (!match(TokenType::Comma)) break;
            }
        }
        expect(TokenType::RParen, "closing module port list");
    }
    expect(TokenType::Semicolon, "after module header");

    while (!check(TokenType::KwEndmodule)) {
        if (check(TokenType::EndOfFile)) error("unexpected end of file inside module");

        if (match(TokenType::KwInput)) {
            parse_port_decl(module, PortDirection::Input);
        } else if (match(TokenType::KwOutput)) {
            parse_port_decl(module, PortDirection::Output);
        } else if (match(TokenType::KwWire)) {
            parse_net_decl(module, /*is_reg=*/false);
        } else if (match(TokenType::KwReg)) {
            parse_net_decl(module, /*is_reg=*/true);
        } else if (match(TokenType::KwAssign)) {
            parse_assign(module);
        } else {
            error("unexpected token '" + peek().text + "' in module body");
        }
    }
    expect(TokenType::KwEndmodule, "to close module");
    return module;
}

// --- Expression grammar (highest to lowest precedence in the call chain) ---

ExprPtr Parser::parse_expr() { return parse_ternary(); }

ExprPtr Parser::parse_ternary() {
    ExprPtr cond = parse_logical_or();
    if (match(TokenType::Question)) {
        ExprPtr then_expr = parse_expr();
        expect(TokenType::Colon, "in ternary expression");
        ExprPtr else_expr = parse_expr();
        return std::make_unique<TernaryExpr>(std::move(cond), std::move(then_expr), std::move(else_expr));
    }
    return cond;
}

ExprPtr Parser::parse_logical_or() {
    ExprPtr lhs = parse_logical_and();
    while (match(TokenType::PipePipe)) {
        ExprPtr rhs = parse_logical_and();
        lhs = std::make_unique<BinaryExpr>(BinOp::LogOr, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parse_logical_and() {
    ExprPtr lhs = parse_bitwise_or();
    while (match(TokenType::AmpAmp)) {
        ExprPtr rhs = parse_bitwise_or();
        lhs = std::make_unique<BinaryExpr>(BinOp::LogAnd, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parse_bitwise_or() {
    ExprPtr lhs = parse_bitwise_xor();
    while (check(TokenType::Pipe)) {
        advance();
        ExprPtr rhs = parse_bitwise_xor();
        lhs = std::make_unique<BinaryExpr>(BinOp::Or, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parse_bitwise_xor() {
    ExprPtr lhs = parse_bitwise_and();
    while (check(TokenType::Caret)) {
        advance();
        ExprPtr rhs = parse_bitwise_and();
        lhs = std::make_unique<BinaryExpr>(BinOp::Xor, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parse_bitwise_and() {
    ExprPtr lhs = parse_equality();
    while (check(TokenType::Amp)) {
        advance();
        ExprPtr rhs = parse_equality();
        lhs = std::make_unique<BinaryExpr>(BinOp::And, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parse_equality() {
    ExprPtr lhs = parse_relational();
    for (;;) {
        if (match(TokenType::EqEq)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Eq, std::move(lhs), parse_relational());
        } else if (match(TokenType::NotEq)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Neq, std::move(lhs), parse_relational());
        } else {
            break;
        }
    }
    return lhs;
}

ExprPtr Parser::parse_relational() {
    ExprPtr lhs = parse_shift();
    for (;;) {
        if (match(TokenType::Le)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Le, std::move(lhs), parse_shift());
        } else if (match(TokenType::Ge)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Ge, std::move(lhs), parse_shift());
        } else if (match(TokenType::Lt)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Lt, std::move(lhs), parse_shift());
        } else if (match(TokenType::Gt)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Gt, std::move(lhs), parse_shift());
        } else {
            break;
        }
    }
    return lhs;
}

ExprPtr Parser::parse_shift() {
    ExprPtr lhs = parse_additive();
    for (;;) {
        if (match(TokenType::Shl)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Shl, std::move(lhs), parse_additive());
        } else if (match(TokenType::Shr)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Shr, std::move(lhs), parse_additive());
        } else {
            break;
        }
    }
    return lhs;
}

ExprPtr Parser::parse_additive() {
    ExprPtr lhs = parse_multiplicative();
    for (;;) {
        if (match(TokenType::Plus)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Add, std::move(lhs), parse_multiplicative());
        } else if (match(TokenType::Minus)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Sub, std::move(lhs), parse_multiplicative());
        } else {
            break;
        }
    }
    return lhs;
}

ExprPtr Parser::parse_multiplicative() {
    ExprPtr lhs = parse_unary();
    for (;;) {
        if (match(TokenType::Star)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Mul, std::move(lhs), parse_unary());
        } else if (match(TokenType::Slash)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Div, std::move(lhs), parse_unary());
        } else if (match(TokenType::Percent)) {
            lhs = std::make_unique<BinaryExpr>(BinOp::Mod, std::move(lhs), parse_unary());
        } else {
            break;
        }
    }
    return lhs;
}

ExprPtr Parser::parse_unary() {
    if (match(TokenType::Minus)) return std::make_unique<UnaryExpr>(UnaryOp::Neg, parse_unary());
    if (match(TokenType::Tilde)) return std::make_unique<UnaryExpr>(UnaryOp::BitNot, parse_unary());
    if (match(TokenType::Bang)) return std::make_unique<UnaryExpr>(UnaryOp::LogNot, parse_unary());
    if (match(TokenType::Amp)) return std::make_unique<UnaryExpr>(UnaryOp::ReduceAnd, parse_unary());
    if (match(TokenType::Pipe)) return std::make_unique<UnaryExpr>(UnaryOp::ReduceOr, parse_unary());
    if (match(TokenType::Caret)) return std::make_unique<UnaryExpr>(UnaryOp::ReduceXor, parse_unary());
    return parse_primary();
}

ExprPtr Parser::parse_concat() {
    // Already consumed '{'. Parses comma-separated expressions until '}'.
    std::vector<ExprPtr> parts;
    parts.push_back(parse_expr());
    while (match(TokenType::Comma)) {
        parts.push_back(parse_expr());
    }
    expect(TokenType::RBrace, "closing concatenation");
    return std::make_unique<ConcatExpr>(std::move(parts));
}

ExprPtr Parser::parse_primary() {
    if (match(TokenType::LParen)) {
        ExprPtr e = parse_expr();
        expect(TokenType::RParen, "closing parenthesis");
        return e;
    }
    if (match(TokenType::LBrace)) {
        return parse_concat();
    }
    if (check(TokenType::Number)) {
        Token t = advance();
        auto [width, value] = parse_number_literal(t.text);
        return std::make_unique<NumberExpr>(t.text, width, value);
    }
    if (check(TokenType::Identifier)) {
        Token t = advance();
        return std::make_unique<IdentExpr>(t.text);
    }
    error("expected expression but found '" + peek().text + "'");
}

} // namespace vlogsim
