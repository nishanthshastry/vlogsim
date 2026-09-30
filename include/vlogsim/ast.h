#pragma once

#include <memory>
#include <string>
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
    // Raw literal text, e.g. "8'hFF", "1010", "4'b1010".
    std::string text;
    int width = -1;       // -1 means unsized (inferred)
    uint64_t value = 0;   // parsed two-state value
    explicit NumberExpr(std::string t, int w, uint64_t v) : text(std::move(t)), width(w), value(v) {}
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

struct Module {
    std::string name;
    std::vector<Port> ports;
    std::vector<NetDecl> wires;
    std::vector<NetDecl> regs;
    std::vector<ContinuousAssign> assigns;
};

} // namespace vlogsim
