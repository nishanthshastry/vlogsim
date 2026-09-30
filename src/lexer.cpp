#include "vlogsim/lexer.h"

#include <cctype>
#include <unordered_map>

namespace vlogsim {

namespace {
const std::unordered_map<std::string, TokenType>& keywords() {
    static const std::unordered_map<std::string, TokenType> kKeywords = {
        {"module", TokenType::KwModule},
        {"endmodule", TokenType::KwEndmodule},
        {"input", TokenType::KwInput},
        {"output", TokenType::KwOutput},
        {"wire", TokenType::KwWire},
        {"reg", TokenType::KwReg},
        {"assign", TokenType::KwAssign},
        {"always", TokenType::KwAlways},
        {"posedge", TokenType::KwPosedge},
        {"negedge", TokenType::KwNegedge},
        {"if", TokenType::KwIf},
        {"else", TokenType::KwElse},
        {"case", TokenType::KwCase},
        {"endcase", TokenType::KwEndcase},
        {"default", TokenType::KwDefault},
        {"parameter", TokenType::KwParameter},
        {"begin", TokenType::KwBegin},
        {"end", TokenType::KwEnd},
        {"initial", TokenType::KwInitial},
    };
    return kKeywords;
}
} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    if (idx >= source_.size()) return '\0';
    return source_[idx];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (peek() != expected) return false;
    advance();
    return true;
}

void Lexer::skip_whitespace_and_comments() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (peek() != '\n' && peek() != '\0') advance();
        } else if (c == '/' && peek(1) == '*') {
            advance();
            advance();
            while (!(peek() == '*' && peek(1) == '/') && peek() != '\0') advance();
            if (peek() != '\0') {
                advance();
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::make_token(TokenType type, const std::string& text) {
    Token t;
    t.type = type;
    t.text = text;
    t.line = line_;
    t.column = column_;
    return t;
}

Token Lexer::lex_identifier_or_keyword() {
    size_t start = pos_;
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == '$') {
        advance();
    }
    std::string text = source_.substr(start, pos_ - start);
    auto it = keywords().find(text);
    if (it != keywords().end()) return make_token(it->second, text);
    return make_token(TokenType::Identifier, text);
}

Token Lexer::lex_number() {
    // Handles plain decimal literals and sized/based literals such as
    // 8'hFF, 4'b1010, 3'd5. Underscores as digit separators are accepted.
    size_t start = pos_;
    while (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_') advance();

    if (peek() == '\'') {
        advance(); // consume '
        if (peek() == 's' || peek() == 'S') advance(); // signed marker, ignored for now
        if (peek() != '\0') advance(); // base character: b/o/d/h
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == 'x' ||
               peek() == 'X' || peek() == 'z' || peek() == 'Z') {
            advance();
        }
    }
    std::string text = source_.substr(start, pos_ - start);
    return make_token(TokenType::Number, text);
}

Token Lexer::next_token() {
    skip_whitespace_and_comments();
    if (pos_ >= source_.size()) return make_token(TokenType::EndOfFile, "");

    char c = peek();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return lex_identifier_or_keyword();
    if (std::isdigit(static_cast<unsigned char>(c))) return lex_number();

    advance();
    switch (c) {
        case '(': return make_token(TokenType::LParen, "(");
        case ')': return make_token(TokenType::RParen, ")");
        case '[': return make_token(TokenType::LBracket, "[");
        case ']': return make_token(TokenType::RBracket, "]");
        case '{': return make_token(TokenType::LBrace, "{");
        case '}': return make_token(TokenType::RBrace, "}");
        case ';': return make_token(TokenType::Semicolon, ";");
        case ',': return make_token(TokenType::Comma, ",");
        case ':': return make_token(TokenType::Colon, ":");
        case '@': return make_token(TokenType::At, "@");
        case '#': return make_token(TokenType::Hash, "#");
        case '.': return make_token(TokenType::Dot, ".");
        case '+': return make_token(TokenType::Plus, "+");
        case '-': return make_token(TokenType::Minus, "-");
        case '*': return make_token(TokenType::Star, "*");
        case '/': return make_token(TokenType::Slash, "/");
        case '%': return make_token(TokenType::Percent, "%");
        case '~': return make_token(TokenType::Tilde, "~");
        case '?': return make_token(TokenType::Question, "?");
        case '=':
            if (match('=')) return make_token(TokenType::EqEq, "==");
            return make_token(TokenType::Assign, "=");
        case '!':
            if (match('=')) return make_token(TokenType::NotEq, "!=");
            return make_token(TokenType::Bang, "!");
        case '&':
            if (match('&')) return make_token(TokenType::AmpAmp, "&&");
            return make_token(TokenType::Amp, "&");
        case '|':
            if (match('|')) return make_token(TokenType::PipePipe, "||");
            return make_token(TokenType::Pipe, "|");
        case '^': return make_token(TokenType::Caret, "^");
        case '<':
            if (match('=')) return make_token(TokenType::Le, "<=");
            if (match('<')) return make_token(TokenType::Shl, "<<");
            return make_token(TokenType::Lt, "<");
        case '>':
            if (match('=')) return make_token(TokenType::Ge, ">=");
            if (match('>')) return make_token(TokenType::Shr, ">>");
            return make_token(TokenType::Gt, ">");
        default:
            return make_token(TokenType::Invalid, std::string(1, c));
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    for (;;) {
        Token t = next_token();
        tokens.push_back(t);
        if (t.type == TokenType::EndOfFile) break;
    }
    return tokens;
}

} // namespace vlogsim
