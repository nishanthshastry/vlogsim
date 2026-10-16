#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "vlogsim/ast.h"

namespace vlogsim {

class ElaborationError : public std::runtime_error {
public:
    explicit ElaborationError(const std::string& msg) : std::runtime_error(msg) {}
};

struct NetInfo {
    std::string name;
    int width = 1;
    bool is_input = false;
    bool is_output = false;
};

// Flattened, validated view of a single module: every net has a resolved
// width and every assign target and RHS identifier is checked against the
// declared port/wire/reg set. Hierarchy flattening (module instantiation)
// is a Milestone-3 concern and out of scope here.
class ElaboratedModule {
public:
    const std::string& name() const { return name_; }
    const std::unordered_map<std::string, NetInfo>& nets() const { return nets_; }
    const Module& source() const { return *source_; }

    bool has_net(const std::string& n) const { return nets_.count(n) != 0; }
    const NetInfo& net(const std::string& n) const { return nets_.at(n); }

private:
    friend class Elaborator;
    std::string name_;
    std::unordered_map<std::string, NetInfo> nets_;
    const Module* source_ = nullptr;
};

class Elaborator {
public:
    ElaboratedModule elaborate(const Module& module);

private:
    void check_expr(const Expr* e, const ElaboratedModule& em);
    void check_stmt(const Stmt* s, const ElaboratedModule& em);
};

} // namespace vlogsim
