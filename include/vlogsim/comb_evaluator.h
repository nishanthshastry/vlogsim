#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "vlogsim/ast.h"
#include "vlogsim/elaborator.h"

namespace vlogsim {

class EvalError : public std::runtime_error {
public:
    explicit EvalError(const std::string& msg) : std::runtime_error(msg) {}
};

// Evaluates the continuous assigns of a combinational (acyclic) module.
// This is the Milestone-1 "assign evaluation" step: not yet a scheduler,
// just a recursive, memoized resolver of net -> value good enough to check
// an adder or a mux against expected truth tables.
class CombEvaluator {
public:
    explicit CombEvaluator(const ElaboratedModule& module) : module_(module) {}

    // `inputs` must provide a value for every net where is_input is true.
    // Returns the two-state value (masked to declared width) of every net
    // in the module, inputs included.
    std::unordered_map<std::string, uint64_t> eval(
        const std::unordered_map<std::string, uint64_t>& inputs) const;

private:
    uint64_t eval_net(const std::string& name, const std::unordered_map<std::string, uint64_t>& inputs,
                       std::unordered_map<std::string, uint64_t>& cache,
                       std::unordered_map<std::string, bool>& in_progress) const;
    uint64_t eval_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& inputs,
                        std::unordered_map<std::string, uint64_t>& cache,
                        std::unordered_map<std::string, bool>& in_progress) const;

    const ElaboratedModule& module_;
};

} // namespace vlogsim
