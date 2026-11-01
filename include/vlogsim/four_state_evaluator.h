// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "vlogsim/ast.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/value.h"

namespace vlogsim {

class FourStateEvalError : public std::runtime_error {
public:
    explicit FourStateEvalError(const std::string& msg) : std::runtime_error(msg) {}
};

// Milestone-4 combinational evaluator: the same recursive, memoized
// assign-resolution structure as CombEvaluator, but over FourState values
// with real X-propagation (see value.h) instead of plain uint64_t.
// Kept as a separate evaluator rather than replacing CombEvaluator so the
// Milestone 1-3 two-state evaluators and their tests are unaffected — the
// design doc frames four-state as an upgrade *path* from the two-state
// baseline, not a forced migration of everything at once. Migrating
// SeqEvaluator (register X, reset-to-X-until-first-clock) to four-state
// is a documented follow-up, not yet done here.
class FourStateCombEvaluator {
public:
    explicit FourStateCombEvaluator(const ElaboratedModule& module) : module_(module) {}

    // `inputs` must provide a value for every net where is_input is true.
    // Returns the four-state value (masked to declared width) of every
    // net in the module, inputs included.
    std::unordered_map<std::string, FourState> eval(const std::unordered_map<std::string, FourState>& inputs) const;

private:
    FourState eval_net(const std::string& name, const std::unordered_map<std::string, FourState>& inputs,
                        std::unordered_map<std::string, FourState>& cache,
                        std::unordered_map<std::string, bool>& in_progress) const;
    FourState eval_expr(const Expr* e, const std::unordered_map<std::string, FourState>& inputs,
                         std::unordered_map<std::string, FourState>& cache,
                         std::unordered_map<std::string, bool>& in_progress) const;

    const ElaboratedModule& module_;
};

} // namespace vlogsim
