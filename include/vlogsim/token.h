// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace vlogsim {

enum class TokenType {
    // literals / identifiers
    Identifier,
    Number,
    StringLiteral, // "..." — Milestone 5: $display format strings

    // keywords
    KwModule,
    KwEndmodule,
    KwInput,
    KwOutput,
    KwWire,
    KwReg,
    KwAssign,
    KwAlways,
    KwPosedge,
    KwNegedge,
    KwIf,
    KwElse,
    KwCase,
    KwEndcase,
    KwDefault,
    KwParameter,
    KwBegin,
    KwEnd,
    KwInitial,

    // punctuation
    LParen,
    RParen,
    LBracket,
    RBracket,
    LBrace,
    RBrace,
    Semicolon,
    Comma,
    Colon,
    At,
    Hash,
    Dot,

    // operators
    Assign,        // =
    NonBlockAssign, // <=
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Amp,        // &
    AmpAmp,     // &&
    Pipe,       // |
    PipePipe,   // ||
    Caret,      // ^
    Tilde,      // ~
    Bang,       // !
    EqEq,       // ==
    NotEq,      // !=
    Lt,
    Gt,
    Le,
    Ge,
    Shl,        // <<
    Shr,        // >>
    Question,

    EndOfFile,
    Invalid,
};

struct Token {
    TokenType type = TokenType::Invalid;
    std::string text;
    int line = 0;
    int column = 0;
};

const char* token_type_name(TokenType t);

} // namespace vlogsim
