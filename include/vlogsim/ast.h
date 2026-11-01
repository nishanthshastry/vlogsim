// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vlogsim {

enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    And, Or, Xor,          // bitwise
    LogAnd, LogOr,         // logical
    Eq, Neq, Lt, Gt, Le, Ge,
    Shl, Shr,
};

enum class UnaryOp {
    Neg, BitNot, LogNot,
    ReduceAnd, ReduceOr, ReduceXor,
};

struct Expr {
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct IdentExpr : Expr {
    std::string name;
    explicit IdentExpr(std::string n) : name(std::move(n)) {}
};

struct NumberExpr : Expr {
    // Raw literal text, e.g. "8'hFF", "1010", "4'b1010", "4'bxx01".
    std::string text;
    int width = -1;       // -1 means unsized (inferred)
    uint64_t value = 0;   // bit value; meaningful only where `unknown` is 0
    uint64_t unknown = 0; // Milestone 4: bits that are 'x'/'z' in the literal ('z' is treated as 'x' — see value.h)
    explicit NumberExpr(std::string t, int w, uint64_t v, uint64_t u = 0)
        : text(std::move(t)), width(w), value(v), unknown(u) {}
};

struct BinaryExpr : Expr {
    BinOp op;
    ExprPtr lhs;
    ExprPtr rhs;
    BinaryExpr(BinOp o, ExprPtr l, ExprPtr r) : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct UnaryExpr : Expr {
    UnaryOp op;
    ExprPtr operand;
    UnaryExpr(UnaryOp o, ExprPtr e) : op(o), operand(std::move(e)) {}
};

struct TernaryExpr : Expr {
    ExprPtr cond;
    ExprPtr then_expr;
    ExprPtr else_expr;
    TernaryExpr(ExprPtr c, ExprPtr t, ExprPtr e)
        : cond(std::move(c)), then_expr(std::move(t)), else_expr(std::move(e)) {}
};

struct ConcatExpr : Expr {
    std::vector<ExprPtr> parts; // {a, b, c} — a is the most-significant part
    explicit ConcatExpr(std::vector<ExprPtr> p) : parts(std::move(p)) {}
};

enum class PortDirection { Input, Output };

struct Port {
    std::string name;
    PortDirection direction = PortDirection::Input;
    int msb = 0; // width is msb - lsb + 1; scalars are msb=0, lsb=0
    int lsb = 0;
};

struct NetDecl {
    std::string name;
    int msb = 0;
    int lsb = 0;
};

struct ContinuousAssign {
    std::string target;
    ExprPtr rhs;
};

// --- Statements (Milestone 2: clocked always blocks) ---

struct Stmt {
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct BlockingAssignStmt : Stmt {
    std::string target;
    ExprPtr rhs;
    BlockingAssignStmt(std::string t, ExprPtr r) : target(std::move(t)), rhs(std::move(r)) {}
};

struct NonBlockingAssignStmt : Stmt {
    std::string target;
    ExprPtr rhs;
    NonBlockingAssignStmt(std::string t, ExprPtr r) : target(std::move(t)), rhs(std::move(r)) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements; // begin ... end
    explicit BlockStmt(std::vector<StmtPtr> s) : statements(std::move(s)) {}
};

struct IfStmt : Stmt {
    ExprPtr cond;
    StmtPtr then_branch;
    StmtPtr else_branch; // nullptr if no else
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e) : cond(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
};

enum class EdgeKind { Posedge, Negedge };

struct SensitivityItem {
    EdgeKind edge;
    std::string net;
};

struct AlwaysBlock {
    std::vector<SensitivityItem> sensitivity; // e.g. posedge clk [or negedge rst_n]
    StmtPtr body;
};

// --- Testbench support (Milestone 5) ---

struct DelayStmt : Stmt {
    int delay = 0;
    StmtPtr stmt; // nullptr for a bare "#N;"; otherwise the statement that follows the delay, e.g. "#10 a = 1;"
    DelayStmt(int d, StmtPtr s) : delay(d), stmt(std::move(s)) {}
};

struct DisplayStmt : Stmt {
    std::string format;
    std::vector<ExprPtr> args;
    DisplayStmt(std::string f, std::vector<ExprPtr> a) : format(std::move(f)), args(std::move(a)) {}
};

struct FinishStmt : Stmt {};

struct InitialBlock {
    StmtPtr body;
};

// --- Hierarchy (Milestone 3: module instantiation and parameters) ---

struct ParamDecl {
    std::string name;
    ExprPtr default_value; // constant expression, may reference earlier params in the same module
};

struct ModuleInstance {
    std::string module_type;
    std::string instance_name;
    std::vector<std::pair<std::string, ExprPtr>> param_overrides; // #(.NAME(expr), ...)

    // Exactly one of these is populated, depending on connection style.
    std::vector<std::pair<std::string, ExprPtr>> named_connections; // .port(expr)
    std::vector<ExprPtr> positional_connections;                    // (expr1, expr2, ...)
};

struct Module {
    std::string name;
    std::vector<Port> ports;
    std::vector<NetDecl> wires;
    std::vector<NetDecl> regs;
    std::vector<ContinuousAssign> assigns;
    std::vector<AlwaysBlock> always_blocks;
    std::vector<InitialBlock> initial_blocks;
    std::vector<ParamDecl> params;
    std::vector<ModuleInstance> instances;
};

// A parsed source file: every module definition it contains, keyed by name.
using Design = std::unordered_map<std::string, Module>;

} // namespace vlogsim
