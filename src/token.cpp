// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/token.h"

namespace vlogsim {

const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::Identifier: return "Identifier";
        case TokenType::Number: return "Number";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::KwModule: return "module";
        case TokenType::KwEndmodule: return "endmodule";
        case TokenType::KwInput: return "input";
        case TokenType::KwOutput: return "output";
        case TokenType::KwWire: return "wire";
        case TokenType::KwReg: return "reg";
        case TokenType::KwAssign: return "assign";
        case TokenType::KwAlways: return "always";
        case TokenType::KwPosedge: return "posedge";
        case TokenType::KwNegedge: return "negedge";
        case TokenType::KwIf: return "if";
        case TokenType::KwElse: return "else";
        case TokenType::KwCase: return "case";
        case TokenType::KwEndcase: return "endcase";
        case TokenType::KwDefault: return "default";
        case TokenType::KwParameter: return "parameter";
        case TokenType::KwBegin: return "begin";
        case TokenType::KwEnd: return "end";
        case TokenType::KwInitial: return "initial";
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::LBracket: return "[";
        case TokenType::RBracket: return "]";
        case TokenType::LBrace: return "{";
        case TokenType::RBrace: return "}";
        case TokenType::Semicolon: return ";";
        case TokenType::Comma: return ",";
        case TokenType::Colon: return ":";
        case TokenType::At: return "@";
        case TokenType::Hash: return "#";
        case TokenType::Dot: return ".";
        case TokenType::Assign: return "=";
        case TokenType::NonBlockAssign: return "<=";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";
        case TokenType::Amp: return "&";
        case TokenType::AmpAmp: return "&&";
        case TokenType::Pipe: return "|";
        case TokenType::PipePipe: return "||";
        case TokenType::Caret: return "^";
        case TokenType::Tilde: return "~";
        case TokenType::Bang: return "!";
        case TokenType::EqEq: return "==";
        case TokenType::NotEq: return "!=";
        case TokenType::Lt: return "<";
        case TokenType::Gt: return ">";
        case TokenType::Le: return "<=";
        case TokenType::Ge: return ">=";
        case TokenType::Shl: return "<<";
        case TokenType::Shr: return ">>";
        case TokenType::Question: return "?";
        case TokenType::EndOfFile: return "EOF";
        case TokenType::Invalid: return "Invalid";
    }
    return "?";
}

} // namespace vlogsim
