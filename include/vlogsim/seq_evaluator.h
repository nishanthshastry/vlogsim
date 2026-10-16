#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "vlogsim/ast.h"
#include "vlogsim/elaborator.h"

namespace vlogsim {

class SeqEvalError : public std::runtime_error {
public:
    explicit SeqEvalError(const std::string& msg) : std::runtime_error(msg) {}
};

// Milestone-2 sequential engine: fires clocked `always` blocks on edges of
// their sensitivity nets and commits non-blocking assignments in the
// two-phase style described by the design doc's kernel sketch (sample all
// RHS values first, then commit). Blocking assignments inside an always
// block take effect immediately and are visible to later statements in
// the same evaluation step, matching Verilog's active-region semantics
// for a single, race-free step.
//
// This is a flat, single-step scheduler: the caller drives the clock by
// calling step() once per input change (e.g. twice per clock period, for
// the falling and rising edge), and edges are detected by diffing against
// the previous call's primary-input values. A sensitivity net may be a
// primary input directly, or a continuous-assign-driven wire resolving
// back to one (the common case after hierarchy flattening, e.g. `assign
// u0__clk = clk;`) — resolved on demand via eval_net_value.
class SeqEvaluator {
public:
    SeqEvaluator(const ElaboratedModule& module,
                 std::unordered_map<std::string, uint64_t> initial_reg_values = {});

    // Applies new primary-input values, fires any always blocks whose
    // sensitivity edge just occurred, and commits their non-blocking
    // writes. Returns the resulting value of every reg plus the inputs
    // just supplied.
    std::unordered_map<std::string, uint64_t> step(const std::unordered_map<std::string, uint64_t>& primary_inputs);

    uint64_t reg_value(const std::string& name) const;

private:
    bool edge_triggered(const SensitivityItem& item, const std::unordered_map<std::string, uint64_t>& new_inputs) const;
    void exec_stmt(const Stmt* s, std::unordered_map<std::string, uint64_t>& env,
                   std::vector<std::pair<std::string, uint64_t>>& nba_writes) const;
    uint64_t eval_expr(const Expr* e, const std::unordered_map<std::string, uint64_t>& env) const;
    uint64_t eval_net_value(const std::string& name, const std::unordered_map<std::string, uint64_t>& env) const;

    const ElaboratedModule& module_;
    std::unordered_map<std::string, uint64_t> reg_state_;
    std::unordered_map<std::string, uint64_t> prev_inputs_;
    bool has_prev_ = false;
};

} // namespace vlogsim
