// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/levelized_evaluator.h"

#include <functional>

#include "vlogsim/value.h"

namespace vlogsim {

namespace {

void collect_idents(const Expr* e, std::vector<std::string>& out) {
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        out.push_back(id->name);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        collect_idents(bin->lhs.get(), out);
        collect_idents(bin->rhs.get(), out);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        collect_idents(un->operand.get(), out);
    } else if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        collect_idents(tern->cond.get(), out);
        collect_idents(tern->then_expr.get(), out);
        collect_idents(tern->else_expr.get(), out);
    } else if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        for (const ExprPtr& part : cat->parts) collect_idents(part.get(), out);
    }
    // NumberExpr has no identifiers to collect.
}

} // namespace

std::vector<std::string> topological_order(const ElaboratedModule& module) {
    std::unordered_map<std::string, const ContinuousAssign*> driver_of;
    for (const ContinuousAssign& a : module.source().assigns) driver_of[a.target] = &a;

    std::vector<std::string> order;
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> in_progress;

    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited[name]) return;
        auto it = driver_of.find(name);
        if (it == driver_of.end()) return; // primary input or otherwise undriven: nothing to order

        if (in_progress[name]) {
            throw LevelizedEvalError("combinational loop detected through net '" + name + "'");
        }
        in_progress[name] = true;

        std::vector<std::string> deps;
        collect_idents(it->second->rhs.get(), deps);
        for (const std::string& dep : deps) visit(dep);

        in_progress[name] = false;
        visited[name] = true;
        order.push_back(name);
    };

    for (const auto& [name, info] : module.nets()) {
        (void)info;
        visit(name);
    }
    return order;
}

LevelizedEvaluator::LevelizedEvaluator(const ElaboratedModule& module) : module_(module) {
    for (const ContinuousAssign& a : module_.source().assigns) driver_of_[a.target] = &a;
    eval_order_ = topological_order(module_); // throws on a cycle, once, here — not on every eval() call
}

uint64_t LevelizedEvaluator::eval_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& values) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        return num->width > 0 ? mask_to_width(num->value, num->width) : num->value;
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        auto it = values.find(id->name);
        if (it == values.end()) {
            throw LevelizedEvalError("net '" + id->name + "' was not available when needed (not an input and has no driver)");
        }
        return it->second;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        uint64_t v = eval_expr(un->operand.get(), values);
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
                else throw LevelizedEvalError("reduce-and (&) is only supported on identifiers or sized literals");
                return v == mask_for_width(width) ? 1 : 0;
            }
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        uint64_t l = eval_expr(bin->lhs.get(), values);
        uint64_t r = eval_expr(bin->rhs.get(), values);
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
        return eval_expr(tern->cond.get(), values) != 0 ? eval_expr(tern->then_expr.get(), values)
                                                          : eval_expr(tern->else_expr.get(), values);
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        uint64_t result = 0;
        int shift = 0;
        for (auto it = cat->parts.rbegin(); it != cat->parts.rend(); ++it) {
            const Expr* part = it->get();
            int width = 1;
            if (auto* id = dynamic_cast<const IdentExpr*>(part)) width = module_.net(id->name).width;
            else if (auto* num = dynamic_cast<const NumberExpr*>(part)) {
                if (num->width <= 0) throw LevelizedEvalError("concatenation requires sized literals");
                width = num->width;
            } else {
                throw LevelizedEvalError("concatenation parts must be identifiers or sized literals");
            }
            result |= (mask_to_width(eval_expr(part, values), width) << shift);
            shift += width;
        }
        return result;
    }
    throw LevelizedEvalError("unsupported expression node");
}

std::unordered_map<std::string, uint64_t> LevelizedEvaluator::eval(
    const std::unordered_map<std::string, uint64_t>& inputs) const {
    std::unordered_map<std::string, uint64_t> values;
    values.reserve(module_.nets().size());

    for (const auto& [name, info] : module_.nets()) {
        if (!info.is_input) continue;
        auto it = inputs.find(name);
        if (it == inputs.end()) throw LevelizedEvalError("no input value supplied for input net '" + name + "'");
        values[name] = mask_to_width(it->second, info.width);
    }

    for (const std::string& name : eval_order_) {
        const ContinuousAssign* driver = driver_of_.at(name);
        values[name] = mask_to_width(eval_expr(driver->rhs.get(), values), module_.net(name).width);
    }
    return values;
}

} // namespace vlogsim
