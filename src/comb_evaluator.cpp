#include "vlogsim/comb_evaluator.h"

#include "vlogsim/value.h"

namespace vlogsim {

uint64_t CombEvaluator::eval_net(const std::string& name,
                                  const std::unordered_map<std::string, uint64_t>& inputs,
                                  std::unordered_map<std::string, uint64_t>& cache,
                                  std::unordered_map<std::string, bool>& in_progress) const {
    auto cached = cache.find(name);
    if (cached != cache.end()) return cached->second;

    if (!module_.has_net(name)) {
        throw EvalError("net '" + name + "' is not declared in module '" + module_.name() + "'");
    }
    const NetInfo& info = module_.net(name);

    if (info.is_input) {
        auto it = inputs.find(name);
        if (it == inputs.end()) {
            throw EvalError("no input value supplied for input net '" + name + "'");
        }
        uint64_t v = mask_to_width(it->second, info.width);
        cache[name] = v;
        return v;
    }

    if (in_progress[name]) {
        throw EvalError("combinational loop detected through net '" + name + "'");
    }
    in_progress[name] = true;

    const ContinuousAssign* driver = nullptr;
    for (const ContinuousAssign& a : module_.source().assigns) {
        if (a.target == name) {
            driver = &a;
            break;
        }
    }
    if (driver == nullptr) {
        throw EvalError("net '" + name + "' has no driver (not an input and not assigned)");
    }

    uint64_t v = mask_to_width(eval_expr(driver->rhs.get(), inputs, cache, in_progress), info.width);
    in_progress[name] = false;
    cache[name] = v;
    return v;
}

uint64_t CombEvaluator::eval_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& inputs,
                                   std::unordered_map<std::string, uint64_t>& cache,
                                   std::unordered_map<std::string, bool>& in_progress) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        return num->width > 0 ? mask_to_width(num->value, num->width) : num->value;
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        return eval_net(id->name, inputs, cache, in_progress);
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        uint64_t v = eval_expr(un->operand.get(), inputs, cache, in_progress);
        switch (un->op) {
            case UnaryOp::Neg: return static_cast<uint64_t>(-static_cast<int64_t>(v));
            case UnaryOp::BitNot: return ~v;
            case UnaryOp::LogNot: return v == 0 ? 1 : 0;
            case UnaryOp::ReduceOr: return v != 0 ? 1 : 0;
            case UnaryOp::ReduceXor: return __builtin_parityll(v);
            case UnaryOp::ReduceAnd: {
                // Reduce-and needs the operand's true bit width, which this
                // flat evaluator only tracks for identifiers and sized
                // literals; that covers the Milestone-1 subset's usage.
                int width;
                if (auto* id = dynamic_cast<const IdentExpr*>(un->operand.get())) {
                    width = module_.net(id->name).width;
                } else if (auto* num = dynamic_cast<const NumberExpr*>(un->operand.get())) {
                    if (num->width <= 0) throw EvalError("reduce-and on an unsized literal is ambiguous");
                    width = num->width;
                } else {
                    throw EvalError("reduce-and (&) is only supported on identifiers or sized literals in this subset");
                }
                return v == mask_for_width(width) ? 1 : 0;
            }
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        uint64_t l = eval_expr(bin->lhs.get(), inputs, cache, in_progress);
        uint64_t r = eval_expr(bin->rhs.get(), inputs, cache, in_progress);
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
        uint64_t c = eval_expr(tern->cond.get(), inputs, cache, in_progress);
        return c != 0 ? eval_expr(tern->then_expr.get(), inputs, cache, in_progress)
                       : eval_expr(tern->else_expr.get(), inputs, cache, in_progress);
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        uint64_t result = 0;
        int shift = 0;
        // Concatenation is MSB-first; walk parts right-to-left, accumulating
        // shift by each part's known width.
        for (auto it = cat->parts.rbegin(); it != cat->parts.rend(); ++it) {
            const Expr* part = it->get();
            int width = 1;
            if (auto* id = dynamic_cast<const IdentExpr*>(part)) {
                width = module_.net(id->name).width;
            } else if (auto* num = dynamic_cast<const NumberExpr*>(part)) {
                if (num->width <= 0) {
                    throw EvalError("concatenation requires sized literals; found unsized '" + num->text + "'");
                }
                width = num->width;
            } else {
                throw EvalError("concatenation parts must be identifiers or sized literals in this subset");
            }
            uint64_t v = mask_to_width(eval_expr(part, inputs, cache, in_progress), width);
            result |= (v << shift);
            shift += width;
        }
        return result;
    }
    throw EvalError("unsupported expression node");
}

std::unordered_map<std::string, uint64_t> CombEvaluator::eval(
    const std::unordered_map<std::string, uint64_t>& inputs) const {
    std::unordered_map<std::string, uint64_t> cache;
    std::unordered_map<std::string, bool> in_progress;
    for (const auto& [name, info] : module_.nets()) {
        eval_net(name, inputs, cache, in_progress);
    }
    return cache;
}

} // namespace vlogsim
