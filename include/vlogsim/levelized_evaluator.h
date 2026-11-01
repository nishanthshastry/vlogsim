// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "vlogsim/ast.h"
#include "vlogsim/elaborator.h"

namespace vlogsim {

class LevelizedEvalError : public std::runtime_error {
public:
    explicit LevelizedEvalError(const std::string& msg) : std::runtime_error(msg) {}
};

// Computes a topological order of every continuous-assign-driven net in
// `module`, such that each net appears after every net its driving
// expression reads. Throws LevelizedEvalError on a combinational cycle.
// Shared by LevelizedEvaluator and CodeGenerator, which both need the
// same ordering but walk it differently (one evaluates net-by-net, the
// other emits one C++ statement per net).
std::vector<std::string> topological_order(const ElaboratedModule& module);

// Milestone-6 performance upgrade over CombEvaluator: instead of
// resolving each net's driver and checking for cycles on every eval()
// call (CombEvaluator's recursive, per-call memoized approach),
// LevelizedEvaluator computes the dependency order once, in the
// constructor, and every eval() call is then a single linear pass
// through that precomputed order — no recursion, no per-call cycle
// bookkeeping. This is the "levelized evaluation... one pass per clock
// edge" upgrade path the design doc describes for the baseline
// event-driven interpreter.
class LevelizedEvaluator {
public:
    explicit LevelizedEvaluator(const ElaboratedModule& module);

    std::unordered_map<std::string, uint64_t> eval(const std::unordered_map<std::string, uint64_t>& inputs) const;

private:
    uint64_t eval_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& values) const;

    const ElaboratedModule& module_;
    std::vector<std::string> eval_order_;                             // non-input nets, dependency-ordered
    std::unordered_map<std::string, const ContinuousAssign*> driver_of_; // precomputed once, not per eval() call
};

} // namespace vlogsim
