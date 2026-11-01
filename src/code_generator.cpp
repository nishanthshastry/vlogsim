// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/code_generator.h"

#include <sstream>

#include "vlogsim/levelized_evaluator.h"
#include "vlogsim/value.h"

namespace vlogsim {

std::string CodeGenerator::hex_mask(int width) {
    uint64_t m = mask_for_width(width);
    std::ostringstream s;
    s << "0x" << std::hex << m << "ULL";
    return s.str();
}

std::string CodeGenerator::emit_expr(const Expr* e) const {
    if (auto* num = dynamic_cast<const NumberExpr*>(e)) {
        std::ostringstream s;
        s << "(UINT64_C(" << num->value << ")";
        if (num->width > 0) s << " & " << hex_mask(num->width);
        s << ")";
        return s.str();
    }
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        return id->name;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(e)) {
        std::string operand = emit_expr(un->operand.get());
        switch (un->op) {
            case UnaryOp::Neg: return "((uint64_t)(-(int64_t)(" + operand + ")))";
            case UnaryOp::BitNot: return "(~(" + operand + "))";
            case UnaryOp::LogNot: return "(((" + operand + ") == 0) ? 1ULL : 0ULL)";
            case UnaryOp::ReduceOr: return "(((" + operand + ") != 0) ? 1ULL : 0ULL)";
            case UnaryOp::ReduceXor: return "((uint64_t)__builtin_parityll(" + operand + "))";
            case UnaryOp::ReduceAnd: {
                int width;
                if (auto* id = dynamic_cast<const IdentExpr*>(un->operand.get())) width = module_.net(id->name).width;
                else if (auto* num = dynamic_cast<const NumberExpr*>(un->operand.get())) width = num->width > 0 ? num->width : 64;
                else throw CodeGenError("reduce-and (&) is only supported on identifiers or sized literals");
                return "(((" + operand + ") == " + hex_mask(width) + ") ? 1ULL : 0ULL)";
            }
        }
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(e)) {
        std::string l = emit_expr(bin->lhs.get());
        std::string r = emit_expr(bin->rhs.get());
        switch (bin->op) {
            case BinOp::Add: return "((" + l + ") + (" + r + "))";
            case BinOp::Sub: return "((" + l + ") - (" + r + "))";
            case BinOp::Mul: return "((" + l + ") * (" + r + "))";
            case BinOp::Div: return "(((" + r + ") == 0) ? 0ULL : (" + l + ") / (" + r + "))";
            case BinOp::Mod: return "(((" + r + ") == 0) ? 0ULL : (" + l + ") % (" + r + "))";
            case BinOp::And: return "((" + l + ") & (" + r + "))";
            case BinOp::Or: return "((" + l + ") | (" + r + "))";
            case BinOp::Xor: return "((" + l + ") ^ (" + r + "))";
            case BinOp::LogAnd: return "((((" + l + ") != 0) && ((" + r + ") != 0)) ? 1ULL : 0ULL)";
            case BinOp::LogOr: return "((((" + l + ") != 0) || ((" + r + ") != 0)) ? 1ULL : 0ULL)";
            case BinOp::Eq: return "(((" + l + ") == (" + r + ")) ? 1ULL : 0ULL)";
            case BinOp::Neq: return "(((" + l + ") != (" + r + ")) ? 1ULL : 0ULL)";
            case BinOp::Lt: return "(((" + l + ") < (" + r + ")) ? 1ULL : 0ULL)";
            case BinOp::Gt: return "(((" + l + ") > (" + r + ")) ? 1ULL : 0ULL)";
            case BinOp::Le: return "(((" + l + ") <= (" + r + ")) ? 1ULL : 0ULL)";
            case BinOp::Ge: return "(((" + l + ") >= (" + r + ")) ? 1ULL : 0ULL)";
            case BinOp::Shl: return "((" + l + ") << (" + r + "))";
            case BinOp::Shr: return "((" + l + ") >> (" + r + "))";
        }
    }
    if (auto* tern = dynamic_cast<const TernaryExpr*>(e)) {
        return "(((" + emit_expr(tern->cond.get()) + ") != 0) ? (" + emit_expr(tern->then_expr.get()) + ") : (" +
               emit_expr(tern->else_expr.get()) + "))";
    }
    if (auto* cat = dynamic_cast<const ConcatExpr*>(e)) {
        std::vector<std::string> pieces;
        int shift = 0;
        for (auto it = cat->parts.rbegin(); it != cat->parts.rend(); ++it) {
            const Expr* part = it->get();
            int width = 1;
            if (auto* id = dynamic_cast<const IdentExpr*>(part)) width = module_.net(id->name).width;
            else if (auto* num = dynamic_cast<const NumberExpr*>(part)) {
                if (num->width <= 0) throw CodeGenError("concatenation requires sized literals");
                width = num->width;
            } else {
                throw CodeGenError("concatenation parts must be identifiers or sized literals");
            }
            std::ostringstream piece;
            piece << "(((" << emit_expr(part) << ") & " << hex_mask(width) << ") << " << shift << ")";
            pieces.push_back(piece.str());
            shift += width;
        }
        std::ostringstream joined;
        joined << "(";
        for (size_t i = 0; i < pieces.size(); ++i) {
            if (i != 0) joined << " | ";
            joined << pieces[i];
        }
        joined << ")";
        return joined.str();
    }
    throw CodeGenError("unsupported expression node during code generation");
}

std::string CodeGenerator::generate(std::string function_name) const {
    if (function_name.empty()) function_name = "eval_" + module_.name();

    std::vector<std::string> order = topological_order(module_);
    std::unordered_map<std::string, const ContinuousAssign*> driver_of;
    for (const ContinuousAssign& a : module_.source().assigns) driver_of[a.target] = &a;

    std::ostringstream out;
    out << "// Auto-generated by vlogsim::CodeGenerator for module '" << module_.name() << "'.\n";
    out << "// Do not edit by hand; regenerate from the source Verilog instead.\n";
    out << "#include <cstdint>\n#include <string>\n#include <unordered_map>\n\n";
    out << "std::unordered_map<std::string, uint64_t> " << function_name
        << "(const std::unordered_map<std::string, uint64_t>& in) {\n";

    for (const auto& [name, info] : module_.nets()) {
        if (!info.is_input) continue;
        out << "    uint64_t " << name << " = in.at(\"" << name << "\") & " << hex_mask(info.width) << ";\n";
    }

    for (const std::string& name : order) {
        const NetInfo& info = module_.net(name);
        const ContinuousAssign* a = driver_of.at(name);
        out << "    uint64_t " << name << " = (" << emit_expr(a->rhs.get()) << ") & " << hex_mask(info.width) << ";\n";
    }

    out << "    return {\n";
    bool first = true;
    for (const auto& [name, info] : module_.nets()) {
        (void)info;
        out << (first ? "" : ",\n") << "        {\"" << name << "\", " << name << "}";
        first = false;
    }
    out << "\n    };\n}\n";
    return out.str();
}

} // namespace vlogsim
