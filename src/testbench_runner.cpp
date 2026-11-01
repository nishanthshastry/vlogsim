// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/testbench_runner.h"

#include <algorithm>
#include <sstream>

#include "vlogsim/value.h"

namespace vlogsim {

TestbenchRunner::TestbenchRunner(const ElaboratedModule& module, std::ostream& out) : module_(module), out_(out) {
    for (const NetDecl& r : module_.source().regs) {
        if (!env_.count(r.name)) env_[r.name] = 0;
    }
}

uint64_t TestbenchRunner::value_of(const std::string& net) const {
    auto it = env_.find(net);
    if (it == env_.end()) throw TestbenchError("no value recorded for net '" + net + "'");
    return it->second;
}

uint64_t TestbenchRunner::eval_expr(const Expr* e) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        return num->width > 0 ? mask_to_width(num->value, num->width) : num->value;
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        if (id->name == "$time") return time_;
        auto it = env_.find(id->name);
        if (it != env_.end()) return it->second;
        // Not yet assigned in this testbench: check for a continuous-assign
        // driver (e.g. a DUT's internal wire reached through hierarchy).
        for (const ContinuousAssign& a : module_.source().assigns) {
            if (a.target == id->name) return mask_to_width(eval_expr(a.rhs.get()), module_.net(id->name).width);
        }
        throw TestbenchError("net '" + id->name + "' has no value at time " + std::to_string(time_));
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        uint64_t v = eval_expr(un->operand.get());
        switch (un->op) {
            case UnaryOp::Neg: return static_cast<uint64_t>(-static_cast<int64_t>(v));
            case UnaryOp::BitNot: return ~v;
            case UnaryOp::LogNot: return v == 0 ? 1 : 0;
            case UnaryOp::ReduceOr: return v != 0 ? 1 : 0;
            case UnaryOp::ReduceXor: return __builtin_parityll(v);
            case UnaryOp::ReduceAnd: {
                int width;
                if (auto* id = dynamic_cast<const IdentExpr*>(un->operand.get())) width = module_.net(id->name).width;
                else if (auto* num = dynamic_cast<const NumberExpr*>(un->operand.get())) width = num->width > 0 ? num->width : 64;
                else throw TestbenchError("reduce-and (&) is only supported on identifiers or sized literals");
                return v == mask_for_width(width) ? 1 : 0;
            }
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        uint64_t l = eval_expr(bin->lhs.get());
        uint64_t r = eval_expr(bin->rhs.get());
        switch (bin->op) {
            case BinOp::Add: return l + r;
            case BinOp::Sub: return l - r;
            case BinOp::Mul: return l * r;
            case BinOp::Div: return r == 0 ? 0 : l / r;
            case BinOp::Mod: return r == 0 ? 0 : l % r;
            case BinOp::And: return l & r;
            case BinOp::Or: return l | r;
            case BinOp::Xor: return l ^ r;
            case BinOp::LogAnd: return (l != 0 && r != 0) ? 1 : 0;
            case BinOp::LogOr: return (l != 0 || r != 0) ? 1 : 0;
            case BinOp::Eq: return l == r ? 1 : 0;
            case BinOp::Neq: return l != r ? 1 : 0;
            case BinOp::Lt: return l < r ? 1 : 0;
            case BinOp::Gt: return l > r ? 1 : 0;
            case BinOp::Le: return l <= r ? 1 : 0;
            case BinOp::Ge: return l >= r ? 1 : 0;
            case BinOp::Shl: return l << r;
            case BinOp::Shr: return l >> r;
        }
    }
    if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        return eval_expr(tern->cond.get()) != 0 ? eval_expr(tern->then_expr.get()) : eval_expr(tern->else_expr.get());
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        uint64_t result = 0;
        int shift = 0;
        for (auto it = cat->parts.rbegin(); it != cat->parts.rend(); ++it) {
            const Expr* part = it->get();
            int width = 1;
            if (auto* id = dynamic_cast<const IdentExpr*>(part)) width = module_.net(id->name).width;
            else if (auto* num = dynamic_cast<const NumberExpr*>(part)) {
                if (num->width <= 0) throw TestbenchError("concatenation requires sized literals");
                width = num->width;
            } else {
                throw TestbenchError("concatenation parts must be identifiers or sized literals");
            }
            result |= (mask_to_width(eval_expr(part), width) << shift);
            shift += width;
        }
        return result;
    }
    throw TestbenchError("unsupported expression node in testbench statement");
}

void TestbenchRunner::exec_display(const DisplayStmt& disp) {
    std::ostringstream line;
    size_t arg_index = 0;
    for (size_t i = 0; i < disp.format.size(); ++i) {
        char c = disp.format[i];
        if (c != '%' || i + 1 >= disp.format.size()) {
            line << c;
            continue;
        }
        char spec = disp.format[++i];
        if (spec == '%') {
            line << '%';
            continue;
        }
        if (arg_index >= disp.args.size()) {
            throw TestbenchError("$display format string has more %-specifiers than arguments");
        }
        uint64_t v = eval_expr(disp.args[arg_index++].get());
        switch (spec) {
            case 'd': line << v; break;
            case 'h': line << std::hex << v << std::dec; break;
            case 'o': line << std::oct << v << std::dec; break;
            case 'b': {
                std::string bits;
                if (v == 0) {
                    bits = "0";
                } else {
                    for (uint64_t x = v; x != 0; x >>= 1) bits.push_back((x & 1) ? '1' : '0');
                    std::reverse(bits.begin(), bits.end());
                }
                line << bits;
                break;
            }
            default: throw TestbenchError(std::string("unsupported $display format specifier '%") + spec + "'");
        }
    }
    out_ << line.str() << "\n";
}

void TestbenchRunner::exec_stmt(const Stmt* s) {
    if (auto* blk = dynamic_cast<const BlockStmt*>(s)) {
        for (const StmtPtr& inner : blk->statements) exec_stmt(inner.get());
    } else if (auto* ba = dynamic_cast<const BlockingAssignStmt*>(s)) {
        env_[ba->target] = mask_to_width(eval_expr(ba->rhs.get()), module_.net(ba->target).width);
    } else if (auto* nba = dynamic_cast<const NonBlockingAssignStmt*>(s)) {
        // Testbenches overwhelmingly use blocking assignment for stimulus;
        // non-blocking is treated identically here (immediate write) rather
        // than modeling the full active/NBA region split SeqEvaluator uses
        // for clocked logic — documented Milestone-5 simplification.
        env_[nba->target] = mask_to_width(eval_expr(nba->rhs.get()), module_.net(nba->target).width);
    } else if (auto* ifs = dynamic_cast<const IfStmt*>(s)) {
        if (eval_expr(ifs->cond.get()) != 0) exec_stmt(ifs->then_branch.get());
        else if (ifs->else_branch) exec_stmt(ifs->else_branch.get());
    } else if (auto* delay = dynamic_cast<const DelayStmt*>(s)) {
        time_ += static_cast<uint64_t>(delay->delay);
        if (delay->stmt) exec_stmt(delay->stmt.get());
    } else if (auto* disp = dynamic_cast<const DisplayStmt*>(s)) {
        exec_display(*disp);
    } else if (dynamic_cast<const FinishStmt*>(s)) {
        finished_ = true;
        throw FinishSignal{};
    } else {
        throw TestbenchError("unsupported statement kind in initial block");
    }
}

void TestbenchRunner::run() {
    for (const InitialBlock& block : module_.source().initial_blocks) {
        try {
            exec_stmt(block.body.get());
        } catch (const FinishSignal&) {
            return; // $finish stops the whole simulation, not just this initial block
        }
    }
}

} // namespace vlogsim
