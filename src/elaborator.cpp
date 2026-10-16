#include "vlogsim/elaborator.h"

namespace vlogsim {

namespace {
int width_of(int msb, int lsb) { return (msb >= lsb ? msb - lsb : lsb - msb) + 1; }
} // namespace

ElaboratedModule Elaborator::elaborate(const Module& module) {
    ElaboratedModule em;
    em.name_ = module.name;
    em.source_ = &module;

    for (const Port& p : module.ports) {
        if (em.nets_.count(p.name)) {
            throw ElaborationError("net '" + p.name + "' declared more than once");
        }
        NetInfo info;
        info.name = p.name;
        info.width = width_of(p.msb, p.lsb);
        info.is_input = (p.direction == PortDirection::Input);
        info.is_output = (p.direction == PortDirection::Output);
        em.nets_[p.name] = info;
    }

    for (const NetDecl& w : module.wires) {
        if (em.nets_.count(w.name)) {
            // A wire re-declaring an existing port (common ANSI-vs-non-ANSI
            // pattern for output wires) just inherits the port's role.
            continue;
        }
        NetInfo info;
        info.name = w.name;
        info.width = width_of(w.msb, w.lsb);
        em.nets_[w.name] = info;
    }

    for (const NetDecl& r : module.regs) {
        if (em.nets_.count(r.name)) continue;
        NetInfo info;
        info.name = r.name;
        info.width = width_of(r.msb, r.lsb);
        em.nets_[r.name] = info;
    }

    for (const ContinuousAssign& a : module.assigns) {
        if (!em.has_net(a.target)) {
            throw ElaborationError("assign target '" + a.target + "' is not a declared net");
        }
        check_expr(a.rhs.get(), em);
    }

    for (const AlwaysBlock& block : module.always_blocks) {
        for (const SensitivityItem& item : block.sensitivity) {
            if (!em.has_net(item.net)) {
                throw ElaborationError("sensitivity net '" + item.net + "' is not a declared net");
            }
        }
        check_stmt(block.body.get(), em);
    }

    return em;
}

void Elaborator::check_stmt(const Stmt* s, const ElaboratedModule& em) {
    if (auto* blk = dynamic_cast<const BlockStmt*>(s)) {
        for (const StmtPtr& inner : blk->statements) check_stmt(inner.get(), em);
    } else if (auto* ba = dynamic_cast<const BlockingAssignStmt*>(s)) {
        if (!em.has_net(ba->target)) throw ElaborationError("assignment target '" + ba->target + "' is not a declared net");
        check_expr(ba->rhs.get(), em);
    } else if (auto* nba = dynamic_cast<const NonBlockingAssignStmt*>(s)) {
        if (!em.has_net(nba->target)) throw ElaborationError("assignment target '" + nba->target + "' is not a declared net");
        check_expr(nba->rhs.get(), em);
    } else if (auto* ifs = dynamic_cast<const IfStmt*>(s)) {
        check_expr(ifs->cond.get(), em);
        check_stmt(ifs->then_branch.get(), em);
        if (ifs->else_branch) check_stmt(ifs->else_branch.get(), em);
    }
}

void Elaborator::check_expr(const Expr* e, const ElaboratedModule& em) {
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        if (!em.has_net(id->name)) {
            throw ElaborationError("identifier '" + id->name + "' is not a declared net");
        }
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        check_expr(bin->lhs.get(), em);
        check_expr(bin->rhs.get(), em);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        check_expr(un->operand.get(), em);
    } else if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        check_expr(tern->cond.get(), em);
        check_expr(tern->then_expr.get(), em);
        check_expr(tern->else_expr.get(), em);
    } else if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        for (const ExprPtr& part : cat->parts) check_expr(part.get(), em);
    }
    // NumberExpr needs no validation.
}

} // namespace vlogsim
