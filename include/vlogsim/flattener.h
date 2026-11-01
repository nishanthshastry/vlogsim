// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "vlogsim/ast.h"

namespace vlogsim {

class FlattenError : public std::runtime_error {
public:
    explicit FlattenError(const std::string& msg) : std::runtime_error(msg) {}
};

// Inlines a hierarchical Design into a single, instance-free Module: every
// instance's nets are renamed with an "<instance>__" prefix, port
// connections become continuous assigns bridging parent and child nets,
// and parameters are constant-folded away entirely (an instance's
// parameter overrides and a module's own parameter defaults are both
// constant expressions, resolved here rather than carried into the
// output). The result is an ordinary Module that Elaborator and the
// evaluators consume unchanged — hierarchy is purely a front-end concern.
//
// Parameters affect expression *values* only in this subset (e.g. an
// `assign` RHS or an always-block condition); they do not parameterize
// net/port widths, which keeps width resolution exactly as it was before
// Milestone 3.
class Flattener {
public:
    explicit Flattener(const Design& design) : design_(design) {}

    Module flatten(const std::string& top_module_name);

private:
    const Module& lookup(const std::string& module_type) const;

    // Resolves every ParamDecl of `mod` to a concrete value, given the
    // (possibly empty) set of overrides supplied by the instantiating
    // context. Later params may reference earlier ones by name.
    std::unordered_map<std::string, uint64_t> resolve_params(
        const Module& mod, const std::unordered_map<std::string, uint64_t>& overrides) const;

    // Recursively copies `mod`'s contents into `out`, with every net name
    // prefixed by `prefix` (empty for the top module) and every IdentExpr
    // either prefixed (a net) or replaced by its resolved constant value
    // (a parameter). `connections` maps `mod`'s own port names to the
    // already-prefixed expressions supplying them, in the *parent's*
    // scope; null for the top-level call, where ports are the design's
    // external interface and need no connecting assigns.
    void flatten_into(Module& out, const Module& mod, const std::string& prefix,
                       const std::unordered_map<std::string, uint64_t>& param_values,
                       const std::unordered_map<std::string, const Expr*>* connections);

    ExprPtr rewrite_expr(const Expr* e, const std::string& prefix,
                         const std::unordered_map<std::string, uint64_t>& param_values) const;
    static ExprPtr clone_expr(const Expr* e);
    StmtPtr rewrite_stmt(const Stmt* s, const std::string& prefix,
                        const std::unordered_map<std::string, uint64_t>& param_values) const;

    static void add_net_if_absent(std::vector<NetDecl>& nets, const NetDecl& decl);

    const Design& design_;
};

// Evaluates a constant expression (numbers, named parameters via `env`,
// and the usual arithmetic/bitwise/unary operators). Used for parameter
// defaults and overrides, which Verilog requires to be elaboration-time
// constants — no signal references allowed.
uint64_t eval_const_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& env);

} // namespace vlogsim
