// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/seq_evaluator.h"

#include "vlogsim/value.h"

namespace vlogsim {

SeqEvaluator::SeqEvaluator(const ElaboratedModule& module,
                            std::unordered_map<std::string, uint64_t> initial_reg_values)
    : module_(module), reg_state_(std::move(initial_reg_values)) {
    // Every reg not explicitly initialized starts at 0, matching Verilog's
    // default unknown-as-0 behavior in this two-state Milestone-2 engine
    // (four-state X modeling is Milestone 4).
    for (const NetDecl& r : module_.source().regs) {
        if (!reg_state_.count(r.name)) reg_state_[r.name] = 0;
    }
}

bool SeqEvaluator::edge_triggered(const SensitivityItem& item,
                                   const std::unordered_map<std::string, uint64_t>& new_inputs) const {
    if (!has_prev_) return false; // no prior sample means no transition yet
    // Resolved through eval_net_value rather than a raw map lookup: after
    // flattening, a sensitivity net is often a pass-through wire created by
    // hierarchy (e.g. `assign u0__clk = clk;`), not a primary input itself.
    uint64_t prev_v, new_v;
    try {
        prev_v = eval_net_value(item.net, prev_inputs_);
        new_v = eval_net_value(item.net, new_inputs);
    } catch (const SeqEvalError&) {
        return false; // net not resolvable yet (e.g. first-ever sample)
    }
    if (item.edge == EdgeKind::Posedge) return prev_v == 0 && new_v != 0;
    return prev_v != 0 && new_v == 0;
}

uint64_t SeqEvaluator::eval_net_value(const std::string& name,
                                       const std::unordered_map<std::string, uint64_t>& env) const {
    IdentExpr tmp(name);
    return eval_expr(&tmp, env);
}

uint64_t SeqEvaluator::eval_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& env) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        return num->width > 0 ? mask_to_width(num->value, num->width) : num->value;
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        auto it = env.find(id->name);
        if (it != env.end()) return it->second;
        // Not directly in env (not a primary input or reg this step): check
        // for a continuous-assign driver — hierarchy flattening creates
        // exactly this kind of pass-through wire for port connections — and
        // resolve it on demand.
        for (const ContinuousAssign& a : module_.source().assigns) {
            if (a.target == id->name) {
                return mask_to_width(eval_expr(a.rhs.get(), env), module_.net(id->name).width);
            }
        }
        throw SeqEvalError("net '" + id->name + "' has no value in this evaluation step");
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        uint64_t v = eval_expr(un->operand.get(), env);
        switch (un->op) {
            case UnaryOp::Neg: return static_cast<uint64_t>(-static_cast<int64_t>(v));
            case UnaryOp::BitNot: return ~v;
            case UnaryOp::LogNot: return v == 0 ? 1 : 0;
            case UnaryOp::ReduceOr: return v != 0 ? 1 : 0;
            case UnaryOp::ReduceXor: return __builtin_parityll(v);
            case UnaryOp::ReduceAnd: {
                int width;
                if (auto* id = dynamic_cast<const IdentExpr*>(un->operand.get())) {
                    width = module_.net(id->name).width;
                } else if (auto* num = dynamic_cast<const NumberExpr*>(un->operand.get())) {
                    if (num->width <= 0) throw SeqEvalError("reduce-and on an unsized literal is ambiguous");
                    width = num->width;
                } else {
                    throw SeqEvalError("reduce-and (&) is only supported on identifiers or sized literals");
                }
                return v == mask_for_width(width) ? 1 : 0;
            }
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        uint64_t l = eval_expr(bin->lhs.get(), env);
        uint64_t r = eval_expr(bin->rhs.get(), env);
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
        uint64_t c = eval_expr(tern->cond.get(), env);
        return c != 0 ? eval_expr(tern->then_expr.get(), env) : eval_expr(tern->else_expr.get(), env);
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        uint64_t result = 0;
        int shift = 0;
        for (auto it = cat->parts.rbegin(); it != cat->parts.rend(); ++it) {
            const Expr* part = it->get();
            int width = 1;
            if (auto* id = dynamic_cast<const IdentExpr*>(part)) {
                width = module_.net(id->name).width;
            } else if (auto* num = dynamic_cast<const NumberExpr*>(part)) {
                if (num->width <= 0) throw SeqEvalError("concatenation requires sized literals");
                width = num->width;
            } else {
                throw SeqEvalError("concatenation parts must be identifiers or sized literals");
            }
            uint64_t v = mask_to_width(eval_expr(part, env), width);
            result |= (v << shift);
            shift += width;
        }
        return result;
    }
    throw SeqEvalError("unsupported expression node in always block");
}

void SeqEvaluator::exec_stmt(const Stmt* s, std::unordered_map<std::string, uint64_t>& env,
                              std::vector<std::pair<std::string, uint64_t>>& nba_writes) const {
    if (auto* blk = dynamic_cast<const BlockStmt*>(s)) {
        for (const StmtPtr& inner : blk->statements) exec_stmt(inner.get(), env, nba_writes);
    } else if (auto* ba = dynamic_cast<const BlockingAssignStmt*>(s)) {
        uint64_t v = eval_expr(ba->rhs.get(), env);
        int width = module_.net(ba->target).width;
        env[ba->target] = mask_to_width(v, width);
    } else if (auto* nba = dynamic_cast<const NonBlockingAssignStmt*>(s)) {
        // Sampled now (against the pre-commit env), committed after every
        // triggered always block for this step has run.
        int width = module_.net(nba->target).width;
        uint64_t v = mask_to_width(eval_expr(nba->rhs.get(), env), width);
        nba_writes.emplace_back(nba->target, v);
    } else if (auto* ifs = dynamic_cast<const IfStmt*>(s)) {
        if (eval_expr(ifs->cond.get(), env) != 0) {
            exec_stmt(ifs->then_branch.get(), env, nba_writes);
        } else if (ifs->else_branch) {
            exec_stmt(ifs->else_branch.get(), env, nba_writes);
        }
    } else {
        throw SeqEvalError("unsupported statement kind in always block");
    }
}

std::unordered_map<std::string, uint64_t> SeqEvaluator::step(
    const std::unordered_map<std::string, uint64_t>& primary_inputs) {
    // The env blocking assignments read/write from: primary inputs plus
    // current reg state. Continuous-assign wires are out of scope for
    // always-block bodies in this subset (documented Milestone-2 limit).
    std::unordered_map<std::string, uint64_t> env = reg_state_;
    for (const auto& [name, value] : primary_inputs) env[name] = value;
    std::unordered_map<std::string, uint64_t> regs_before = reg_state_;

    std::vector<std::pair<std::string, uint64_t>> nba_writes;
    for (const AlwaysBlock& block : module_.source().always_blocks) {
        bool fires = false;
        for (const SensitivityItem& item : block.sensitivity) {
            if (edge_triggered(item, primary_inputs)) {
                fires = true;
                break;
            }
        }
        if (fires) exec_stmt(block.body.get(), env, nba_writes);
    }

    // Blocking writes take effect immediately within the step: any reg the
    // env ended up different from its pre-step value was written by a
    // BlockingAssignStmt (exec_stmt mutates env in place for those).
    for (const auto& [name, before] : regs_before) {
        auto it = env.find(name);
        if (it != env.end() && it->second != before) reg_state_[name] = it->second;
    }
    // Two-phase commit: non-blocking writes land last, after every
    // triggered always block has sampled its right-hand side, so they win
    // over any blocking write to the same reg in the same step.
    for (const auto& [target, value] : nba_writes) {
        reg_state_[target] = value;
    }

    prev_inputs_ = primary_inputs;
    has_prev_ = true;

    std::unordered_map<std::string, uint64_t> result = reg_state_;
    for (const auto& [name, value] : primary_inputs) result[name] = value;
    return result;
}

uint64_t SeqEvaluator::reg_value(const std::string& name) const {
    auto it = reg_state_.find(name);
    if (it == reg_state_.end()) throw SeqEvalError("no such reg '" + name + "'");
    return it->second;
}

} // namespace vlogsim
