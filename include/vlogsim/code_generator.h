// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>

#include "vlogsim/ast.h"
#include "vlogsim/elaborator.h"

namespace vlogsim {

class CodeGenError : public std::runtime_error {
public:
    explicit CodeGenError(const std::string& msg) : std::runtime_error(msg) {}
};

// Milestone-6 code generation: emits a self-contained C++ translation
// unit implementing a module's combinational logic as plain local
// variable assignments in topological order (the same order
// LevelizedEvaluator uses) — the "emit C++ from the IR and compile it
// with the system compiler, Verilator-style" upgrade path the design
// doc describes. The generated eval function has no runtime dependency
// on vlogsim itself; it only needs <cstdint>, <string> and
// <unordered_map> from the standard library.
class CodeGenerator {
public:
    explicit CodeGenerator(const ElaboratedModule& module) : module_(module) {}

    // Returns the generated .cpp source text. `function_name` defaults to
    // "eval_<module name>".
    std::string generate(std::string function_name = "") const;

private:
    std::string emit_expr(const Expr* e) const;
    static std::string hex_mask(int width);

    const ElaboratedModule& module_;
};

} // namespace vlogsim
