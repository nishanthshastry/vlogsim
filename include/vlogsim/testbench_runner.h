// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "vlogsim/ast.h"
#include "vlogsim/elaborator.h"

namespace vlogsim {

class TestbenchError : public std::runtime_error {
public:
    explicit TestbenchError(const std::string& msg) : std::runtime_error(msg) {}
};

// Milestone-5 testbench driver: executes every `initial` block in a module
// to completion, in textual order (true parallel/interleaved execution of
// multiple initial blocks — and `forever` loops — are out of scope for
// this milestone; see the README). Supports blocking/non-blocking
// assignment, if/else, begin/end, `#N` delays that advance a simulated
// time counter, `$display` with %d/%b/%h/%o specifiers, `$time`, and
// `$finish`, which unwinds out of all remaining statements immediately.
//
// This runs purely against the module's own regs/wires — it does not
// drive a separately-stepped SeqEvaluator DUT. A testbench that wants to
// clock a hierarchical design should assign to the DUT's clock input
// directly via instantiation (Flattener inlines it into the same flat
// module this class evaluates).
class TestbenchRunner {
public:
    explicit TestbenchRunner(const ElaboratedModule& module, std::ostream& out);

    // Runs every initial block to completion or until $finish fires.
    void run();

    uint64_t time() const { return time_; }
    bool finished() const { return finished_; }
    uint64_t value_of(const std::string& net) const;

private:
    struct FinishSignal {}; // thrown by $finish, caught at run()'s top level

    void exec_stmt(const Stmt* s);
    uint64_t eval_expr(const Expr* e) const;
    void exec_display(const DisplayStmt& disp);

    const ElaboratedModule& module_;
    std::ostream& out_;
    std::unordered_map<std::string, uint64_t> env_;
    uint64_t time_ = 0;
    bool finished_ = false;
};

} // namespace vlogsim
