// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/four_state_evaluator.h"

#include <algorithm>

namespace vlogsim {

FourState FourStateCombEvaluator::eval_net(const std::string& name,
                                            const std::unordered_map<std::string, FourState>& inputs,
                                            std::unordered_map<std::string, FourState>& cache,
                                            std::unordered_map<std::string, bool>& in_progress) const {
    auto cached = cache.find(name);
    if (cached != cache.end()) return cached->second;

    if (!module_.has_net(name)) {
        throw FourStateEvalError("net '" + name + "' is not declared in module '" + module_.name() + "'");
    }
    const NetInfo& info = module_.net(name);

    if (info.is_input) {
        auto it = inputs.find(name);
        if (it == inputs.end()) throw FourStateEvalError("no input value supplied for input net '" + name + "'");
        FourState v = mask_four_state(it->second, info.width);
        cache[name] = v;
        return v;
    }

    if (in_progress[name]) throw FourStateEvalError("combinational loop detected through net '" + name + "'");
    in_progress[name] = true;

    const ContinuousAssign* driver = nullptr;
    for (const ContinuousAssign& a : module_.source().assigns) {
        if (a.target == name) {
            driver = &a;
            break;
        }
    }
    if (driver == nullptr) {
        throw FourStateEvalError("net '" + name + "' has no driver (not an input and not assigned)");
    }

    FourState v = mask_four_state(eval_expr(driver->rhs.get(), inputs, cache, in_progress), info.width);
    in_progress[name] = false;
    cache[name] = v;
    return v;
}

FourState FourStateCombEvaluator::eval_expr(const Expr* e, const std::unordered_map<std::string, FourState>& inputs,
                                             std::unordered_map<std::string, FourState>& cache,
                                             std::unordered_map<std::string, bool>& in_progress) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        int width = num->width > 0 ? num->width : 64;
        return mask_four_state(FourState{num->value, num->unknown}, width);
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        return eval_net(id->name, inputs, cache, in_progress);
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        FourState v = eval_expr(un->operand.get(), inputs, cache, in_progress);
        int width = 64;
        if (auto* id = dynamic_cast<const IdentExpr*>(un->operand.get())) width = module_.net(id->name).width;
        else if (auto* num = dynamic_cast<const NumberExpr*>(un->operand.get())) width = num->width > 0 ? num->width : 64;

        switch (un->op) {
            case UnaryOp::BitNot: return four_not(v, width);
            case UnaryOp::LogNot: {
                TriBool t = four_truth(v, width);
                if (t == TriBool::True) return four_from_tribool(TriBool::False);
                if (t == TriBool::False) return four_from_tribool(TriBool::True);
                return four_from_tribool(TriBool::Unknown);
            }
            case UnaryOp::ReduceAnd: return four_reduce_and(v, width);
            case UnaryOp::ReduceOr: return four_reduce_or(v, width);
            case UnaryOp::ReduceXor: return four_reduce_xor(v, width);
            case UnaryOp::Neg:
                // Arithmetic negation has no absorbing case: any unknown
                // input bit makes the whole result unknown (see value.h).
                return four_has_unknown(v, width) ? four_full_x(width)
                                                   : four_known(static_cast<uint64_t>(-static_cast<int64_t>(v.bits)));
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        auto operand_width = [&](const Expr* operand) {
            if (auto* id = dynamic_cast<const IdentExpr*>(operand)) return module_.net(id->name).width;
            if (auto* num = dynamic_cast<const NumberExpr*>(operand)) return num->width > 0 ? num->width : 64;
            return 64;
        };
        FourState l = eval_expr(bin->lhs.get(), inputs, cache, in_progress);
        FourState r = eval_expr(bin->rhs.get(), inputs, cache, in_progress);
        int width = std::max(operand_width(bin->lhs.get()), operand_width(bin->rhs.get()));

        switch (bin->op) {
            case BinOp::And: return four_and(l, r, width);
            case BinOp::Or: return four_or(l, r, width);
            case BinOp::Xor: return four_xor(l, r, width);
            case BinOp::Eq: return four_eq(l, r, width);
            case BinOp::Neq: {
                FourState eq = four_eq(l, r, width);
                if (eq.unknown != 0) return eq; // X stays X
                return four_known(eq.bits == 0 ? 1 : 0);
            }
            case BinOp::LogAnd: {
                TriBool lt = four_truth(l, width), rt = four_truth(r, width);
                if (lt == TriBool::False || rt == TriBool::False) return four_from_tribool(TriBool::False);
                if (lt == TriBool::True && rt == TriBool::True) return four_from_tribool(TriBool::True);
                return four_from_tribool(TriBool::Unknown);
            }
            case BinOp::LogOr: {
                TriBool lt = four_truth(l, width), rt = four_truth(r, width);
                if (lt == TriBool::True || rt == TriBool::True) return four_from_tribool(TriBool::True);
                if (lt == TriBool::False && rt == TriBool::False) return four_from_tribool(TriBool::False);
                return four_from_tribool(TriBool::Unknown);
            }
            // Arithmetic, shift, and ordered-relational ops: no absorbing
            // case modeled, so any unknown bit in either operand makes the
            // whole result unknown (documented Milestone-4 simplification).
            case BinOp::Add:
            case BinOp::Sub:
            case BinOp::Mul:
            case BinOp::Div:
            case BinOp::Mod:
            case BinOp::Shl:
            case BinOp::Shr:
            case BinOp::Lt:
            case BinOp::Gt:
            case BinOp::Le:
            case BinOp::Ge: {
                if (four_has_unknown(l, width) || four_has_unknown(r, width)) {
                    bool is_relational = bin->op == BinOp::Lt || bin->op == BinOp::Gt || bin->op == BinOp::Le || bin->op == BinOp::Ge;
                    return is_relational ? four_full_x(1) : four_full_x(width);
                }
                uint64_t lv = l.bits, rv = r.bits;
                switch (bin->op) {
                    case BinOp::Add: return four_known(lv + rv);
                    case BinOp::Sub: return four_known(lv - rv);
                    case BinOp::Mul: return four_known(lv * rv);
                    case BinOp::Div: return four_known(rv == 0 ? 0 : lv / rv);
                    case BinOp::Mod: return four_known(rv == 0 ? 0 : lv % rv);
                    case BinOp::Shl: return four_known(lv << rv);
                    case BinOp::Shr: return four_known(lv >> rv);
                    case BinOp::Lt: return four_known(lv < rv ? 1 : 0);
                    case BinOp::Gt: return four_known(lv > rv ? 1 : 0);
                    case BinOp::Le: return four_known(lv <= rv ? 1 : 0);
                    case BinOp::Ge: return four_known(lv >= rv ? 1 : 0);
                    default: break;
                }
                break;
            }
        }
    }
    if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        FourState c = eval_expr(tern->cond.get(), inputs, cache, in_progress);
        TriBool truth = four_truth(c, 64);
        if (truth == TriBool::True) return eval_expr(tern->then_expr.get(), inputs, cache, in_progress);
        if (truth == TriBool::False) return eval_expr(tern->else_expr.get(), inputs, cache, in_progress);

        FourState then_v = eval_expr(tern->then_expr.get(), inputs, cache, in_progress);
        FourState else_v = eval_expr(tern->else_expr.get(), inputs, cache, in_progress);
        auto operand_width = [&](const Expr* operand) {
            if (auto* id = dynamic_cast<const IdentExpr*>(operand)) return module_.net(id->name).width;
            if (auto* num = dynamic_cast<const NumberExpr*>(operand)) return num->width > 0 ? num->width : 64;
            return 64;
        };
        int width = std::max(operand_width(tern->then_expr.get()), operand_width(tern->else_expr.get()));
        return four_merge_on_unknown_cond(then_v, else_v, width);
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        FourState result{0, 0};
        int shift = 0;
        for (auto it = cat->parts.rbegin(); it != cat->parts.rend(); ++it) {
            const Expr* part = it->get();
            int width = 1;
            if (auto* id = dynamic_cast<const IdentExpr*>(part)) {
                width = module_.net(id->name).width;
            } else if (auto* num = dynamic_cast<const NumberExpr*>(part)) {
                if (num->width <= 0) throw FourStateEvalError("concatenation requires sized literals; found unsized '" + num->text + "'");
                width = num->width;
            } else {
                throw FourStateEvalError("concatenation parts must be identifiers or sized literals in this subset");
            }
            FourState v = eval_expr(part, inputs, cache, in_progress);
            result = four_concat_part(result, v, width, shift);
            shift += width;
        }
        return result;
    }
    throw FourStateEvalError("unsupported expression node");
}

std::unordered_map<std::string, FourState> FourStateCombEvaluator::eval(
    const std::unordered_map<std::string, FourState>& inputs) const {
    std::unordered_map<std::string, FourState> cache;
    std::unordered_map<std::string, bool> in_progress;
    for (const auto& [name, info] : module_.nets()) {
        (void)info;
        eval_net(name, inputs, cache, in_progress);
    }
    return cache;
}

} // namespace vlogsim
