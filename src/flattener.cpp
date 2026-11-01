// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/flattener.h"

#include <algorithm>

namespace vlogsim {

uint64_t eval_const_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& env) {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) return num->value;
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        auto it = env.find(id->name);
        if (it == env.end()) {
            throw FlattenError("'" + id->name + "' is not a constant (parameter) expression");
        }
        return it->second;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        uint64_t v = eval_const_expr(un->operand.get(), env);
        switch (un->op) {
            case UnaryOp::Neg: return static_cast<uint64_t>(-static_cast<int64_t>(v));
            case UnaryOp::BitNot: return ~v;
            case UnaryOp::LogNot: return v == 0 ? 1 : 0;
            default: throw FlattenError("unsupported operator in constant expression");
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        uint64_t l = eval_const_expr(bin->lhs.get(), env);
        uint64_t r = eval_const_expr(bin->rhs.get(), env);
        switch (bin->op) {
            case BinOp::Add: return l + r;
            case BinOp::Sub: return l - r;
            case BinOp::Mul: return l * r;
            case BinOp::Div: return r == 0 ? 0 : l / r;
            case BinOp::Mod: return r == 0 ? 0 : l % r;
            case BinOp::And: return l & r;
            case BinOp::Or: return l | r;
            case BinOp::Xor: return l ^ r;
            case BinOp::Shl: return l << r;
            case BinOp::Shr: return l >> r;
            default: throw FlattenError("unsupported operator in constant expression");
        }
    }
    throw FlattenError("expression is not a supported constant form (parameter expressions support numbers, other parameters, and +-*/%&|^~<<>> only)");
}

const Module& Flattener::lookup(const std::string& module_type) const {
    auto it = design_.find(module_type);
    if (it == design_.end()) throw FlattenError("module '" + module_type + "' is not defined in this design");
    return it->second;
}

std::unordered_map<std::string, uint64_t> Flattener::resolve_params(
    const Module& mod, const std::unordered_map<std::string, uint64_t>& overrides) const {
    std::unordered_map<std::string, uint64_t> values;
    for (const ParamDecl& p : mod.params) {
        auto ov = overrides.find(p.name);
        if (ov != overrides.end()) {
            values[p.name] = ov->second;
        } else {
            values[p.name] = eval_const_expr(p.default_value.get(), values);
        }
    }
    return values;
}

void Flattener::add_net_if_absent(std::vector<NetDecl>& nets, const NetDecl& decl) {
    bool exists = std::any_of(nets.begin(), nets.end(), [&](const NetDecl& n) { return n.name == decl.name; });
    if (!exists) nets.push_back(decl);
}

ExprPtr Flattener::clone_expr(const Expr* e) {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        return std::make_unique<NumberExpr>(num->text, num->width, num->value, num->unknown);
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        return std::make_unique<IdentExpr>(id->name);
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        return std::make_unique<UnaryExpr>(un->op, clone_expr(un->operand.get()));
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        return std::make_unique<BinaryExpr>(bin->op, clone_expr(bin->lhs.get()), clone_expr(bin->rhs.get()));
    }
    if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        return std::make_unique<TernaryExpr>(clone_expr(tern->cond.get()), clone_expr(tern->then_expr.get()),
                                             clone_expr(tern->else_expr.get()));
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        std::vector<ExprPtr> parts;
        parts.reserve(cat->parts.size());
        for (const ExprPtr& part : cat->parts) parts.push_back(clone_expr(part.get()));
        return std::make_unique<ConcatExpr>(std::move(parts));
    }
    throw FlattenError("unsupported expression node during flattening");
}

ExprPtr Flattener::rewrite_expr(const Expr* e, const std::string& prefix,
                                const std::unordered_map<std::string, uint64_t>& param_values) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        return std::make_unique<NumberExpr>(num->text, num->width, num->value, num->unknown);
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        if (id->name == "$time") return std::make_unique<IdentExpr>(id->name); // pseudo-identifier, never prefixed
        auto it = param_values.find(id->name);
        if (it != param_values.end()) {
            return std::make_unique<NumberExpr>(std::to_string(it->second), -1, it->second);
        }
        return std::make_unique<IdentExpr>(prefix + id->name);
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        return std::make_unique<UnaryExpr>(un->op, rewrite_expr(un->operand.get(), prefix, param_values));
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        return std::make_unique<BinaryExpr>(bin->op, rewrite_expr(bin->lhs.get(), prefix, param_values),
                                            rewrite_expr(bin->rhs.get(), prefix, param_values));
    }
    if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        return std::make_unique<TernaryExpr>(rewrite_expr(tern->cond.get(), prefix, param_values),
                                             rewrite_expr(tern->then_expr.get(), prefix, param_values),
                                             rewrite_expr(tern->else_expr.get(), prefix, param_values));
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        std::vector<ExprPtr> parts;
        parts.reserve(cat->parts.size());
        for (const ExprPtr& part : cat->parts) parts.push_back(rewrite_expr(part.get(), prefix, param_values));
        return std::make_unique<ConcatExpr>(std::move(parts));
    }
    throw FlattenError("unsupported expression node during flattening");
}

StmtPtr Flattener::rewrite_stmt(const Stmt* s, const std::string& prefix,
                                const std::unordered_map<std::string, uint64_t>& param_values) const {
    if (auto* blk = dynamic_cast<const BlockStmt*>(s)) {
        std::vector<StmtPtr> stmts;
        stmts.reserve(blk->statements.size());
        for (const StmtPtr& inner : blk->statements) stmts.push_back(rewrite_stmt(inner.get(), prefix, param_values));
        return std::make_unique<BlockStmt>(std::move(stmts));
    }
    if (auto* ba = dynamic_cast<const BlockingAssignStmt*>(s)) {
        return std::make_unique<BlockingAssignStmt>(prefix + ba->target, rewrite_expr(ba->rhs.get(), prefix, param_values));
    }
    if (auto* nba = dynamic_cast<const NonBlockingAssignStmt*>(s)) {
        return std::make_unique<NonBlockingAssignStmt>(prefix + nba->target, rewrite_expr(nba->rhs.get(), prefix, param_values));
    }
    if (auto* ifs = dynamic_cast<const IfStmt*>(s)) {
        return std::make_unique<IfStmt>(rewrite_expr(ifs->cond.get(), prefix, param_values),
                                        rewrite_stmt(ifs->then_branch.get(), prefix, param_values),
                                        ifs->else_branch ? rewrite_stmt(ifs->else_branch.get(), prefix, param_values) : nullptr);
    }
    if (auto* delay = dynamic_cast<const DelayStmt*>(s)) {
        return std::make_unique<DelayStmt>(delay->delay,
                                           delay->stmt ? rewrite_stmt(delay->stmt.get(), prefix, param_values) : nullptr);
    }
    if (auto* disp = dynamic_cast<const DisplayStmt*>(s)) {
        std::vector<ExprPtr> args;
        args.reserve(disp->args.size());
        for (const ExprPtr& arg : disp->args) args.push_back(rewrite_expr(arg.get(), prefix, param_values));
        return std::make_unique<DisplayStmt>(disp->format, std::move(args));
    }
    if (dynamic_cast<const FinishStmt*>(s)) {
        return std::make_unique<FinishStmt>();
    }
    throw FlattenError("unsupported statement node during flattening");
}

void Flattener::flatten_into(Module& out, const Module& mod, const std::string& prefix,
                              const std::unordered_map<std::string, uint64_t>& param_values,
                              const std::unordered_map<std::string, const Expr*>* connections) {
    if (!prefix.empty()) {
        for (const Port& port : mod.ports) {
            NetDecl decl{prefix + port.name, port.msb, port.lsb};
            add_net_if_absent(out.wires, decl);

            const Expr* conn_expr = nullptr;
            if (connections) {
                auto it = connections->find(port.name);
                if (it != connections->end()) conn_expr = it->second;
            }
            if (conn_expr == nullptr) {
                throw FlattenError("instance of '" + mod.name + "' has no connection for port '" + port.name + "'");
            }

            if (port.direction == PortDirection::Input) {
                // conn_expr lives in the parent's already-prefixed scope
                // (the caller rewrote it before recursing), so it is cloned
                // as-is rather than rewritten again with this module's own
                // (child) prefix.
                out.assigns.push_back(ContinuousAssign{decl.name, clone_expr(conn_expr)});
            } else {
                auto* id = dynamic_cast<const IdentExpr*>(conn_expr);
                if (id == nullptr) {
                    throw FlattenError("output port '" + port.name + "' on instance must connect to a plain net name in this subset");
                }
                // conn_expr lives in the parent's already-prefixed scope
                // (the caller rewrote it before recursing), so it is used
                // as-is rather than rewritten again here.
                out.assigns.push_back(ContinuousAssign{id->name, std::make_unique<IdentExpr>(decl.name)});
            }
        }
    }

    for (const NetDecl& w : mod.wires) add_net_if_absent(out.wires, NetDecl{prefix + w.name, w.msb, w.lsb});
    for (const NetDecl& r : mod.regs) add_net_if_absent(out.regs, NetDecl{prefix + r.name, r.msb, r.lsb});

    for (const ContinuousAssign& a : mod.assigns) {
        out.assigns.push_back(ContinuousAssign{prefix + a.target, rewrite_expr(a.rhs.get(), prefix, param_values)});
    }

    for (const AlwaysBlock& block : mod.always_blocks) {
        AlwaysBlock new_block;
        for (const SensitivityItem& item : block.sensitivity) {
            new_block.sensitivity.push_back(SensitivityItem{item.edge, prefix + item.net});
        }
        new_block.body = rewrite_stmt(block.body.get(), prefix, param_values);
        out.always_blocks.push_back(std::move(new_block));
    }

    for (const InitialBlock& block : mod.initial_blocks) {
        InitialBlock new_block;
        new_block.body = rewrite_stmt(block.body.get(), prefix, param_values);
        out.initial_blocks.push_back(std::move(new_block));
    }

    for (const ModuleInstance& inst : mod.instances) {
        const Module& child_def = lookup(inst.module_type);
        std::string child_prefix = prefix + inst.instance_name + "__";

        std::unordered_map<std::string, uint64_t> child_overrides;
        for (const auto& [pname, pexpr] : inst.param_overrides) {
            child_overrides[pname] = eval_const_expr(pexpr.get(), param_values);
        }
        std::unordered_map<std::string, uint64_t> child_params = resolve_params(child_def, child_overrides);

        // Materialize the connection expressions in `out`'s (already
        // prefixed) namespace up front, so flatten_into's recursive call
        // can treat `connections` as plain pointers without re-deriving
        // scope at every use.
        std::vector<ExprPtr> owned_conn_exprs;
        std::unordered_map<std::string, const Expr*> child_conn;

        if (!inst.named_connections.empty()) {
            for (const auto& [pname, pexpr] : inst.named_connections) {
                owned_conn_exprs.push_back(rewrite_expr(pexpr.get(), prefix, param_values));
                child_conn[pname] = owned_conn_exprs.back().get();
            }
        } else {
            if (inst.positional_connections.size() != child_def.ports.size()) {
                throw FlattenError("instance '" + inst.instance_name + "' of '" + inst.module_type +
                                    "' supplies " + std::to_string(inst.positional_connections.size()) +
                                    " positional connections but the module has " +
                                    std::to_string(child_def.ports.size()) + " ports");
            }
            for (size_t i = 0; i < inst.positional_connections.size(); ++i) {
                owned_conn_exprs.push_back(rewrite_expr(inst.positional_connections[i].get(), prefix, param_values));
                child_conn[child_def.ports[i].name] = owned_conn_exprs.back().get();
            }
        }

        flatten_into(out, child_def, child_prefix, child_params, &child_conn);
    }
}

Module Flattener::flatten(const std::string& top_module_name) {
    const Module& top = lookup(top_module_name);
    Module out;
    out.name = top.name;
    out.ports = top.ports;

    std::unordered_map<std::string, uint64_t> top_params = resolve_params(top, {});
    flatten_into(out, top, /*prefix=*/"", top_params, /*connections=*/nullptr);
    return out;
}

} // namespace vlogsim
