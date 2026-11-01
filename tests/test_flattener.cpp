// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/flattener.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "vlogsim/seq_evaluator.h"
#include "mini_test.h"

using namespace vlogsim;

static Design parse_design(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    return parser.parse_design();
}

static void test_two_instance_combinational_hierarchy() {
    // top = (a & b) | c, built from two single-gate submodules.
    std::string src = R"(
        module and_gate(input a, input b, output y);
            assign y = a & b;
        endmodule

        module or_gate(input a, input b, output y);
            assign y = a | b;
        endmodule

        module top(input a, input b, input c, output y);
            wire ab;
            and_gate u_and(.a(a), .b(b), .y(ab));
            or_gate u_or(.a(ab), .b(c), .y(y));
        endmodule
    )";
    Design design = parse_design(src);
    CHECK_EQ(design.size(), 3u);

    Flattener flattener(design);
    Module flat = flattener.flatten("top");
    CHECK(flat.instances.empty()); // fully inlined

    Elaborator elaborator;
    ElaboratedModule em = elaborator.elaborate(flat);
    CombEvaluator eval(em);

    for (uint64_t a = 0; a <= 1; ++a) {
        for (uint64_t b = 0; b <= 1; ++b) {
            for (uint64_t c = 0; c <= 1; ++c) {
                auto result = eval.eval({{"a", a}, {"b", b}, {"c", c}});
                uint64_t expected = (a & b) | c;
                CHECK_EQ(result.at("y"), expected);
            }
        }
    }
}

static void test_parameter_overrides_per_instance() {
    std::string src = R"(
        module add_const(input [7:0] in, output [7:0] out);
            parameter OFFSET = 1;
            assign out = in + OFFSET;
        endmodule

        module top(input [7:0] x, output [7:0] y1, output [7:0] y2);
            add_const #(.OFFSET(5)) u1(.in(x), .out(y1));
            add_const #(.OFFSET(10)) u2(.in(x), .out(y2));
        endmodule
    )";
    Design design = parse_design(src);
    Flattener flattener(design);
    Module flat = flattener.flatten("top");

    Elaborator elaborator;
    ElaboratedModule em = elaborator.elaborate(flat);
    CombEvaluator eval(em);

    auto result = eval.eval({{"x", 3}});
    CHECK_EQ(result.at("y1"), 8u);  // 3 + 5
    CHECK_EQ(result.at("y2"), 13u); // 3 + 10
}

static void test_nested_two_level_hierarchy() {
    // top -> buf2 -> inv, inv : a non-inverting buffer built from two NOTs.
    std::string src = R"(
        module inv(input a, output y);
            assign y = ~a;
        endmodule

        module buf2(input a, output y);
            wire mid;
            inv u1(.a(a), .y(mid));
            inv u2(.a(mid), .y(y));
        endmodule

        module top(input a, output y);
            buf2 u(.a(a), .y(y));
        endmodule
    )";
    Design design = parse_design(src);
    Flattener flattener(design);
    Module flat = flattener.flatten("top");

    Elaborator elaborator;
    ElaboratedModule em = elaborator.elaborate(flat);
    CombEvaluator eval(em);

    CHECK_EQ(eval.eval({{"a", 0}}).at("y"), 0u);
    CHECK_EQ(eval.eval({{"a", 1}}).at("y"), 1u);
}

static void test_positional_connections() {
    std::string src = R"(
        module and_gate(input a, input b, output y);
            assign y = a & b;
        endmodule

        module top(input a, input b, output y);
            and_gate u(a, b, y);
        endmodule
    )";
    Design design = parse_design(src);
    Flattener flattener(design);
    Module flat = flattener.flatten("top");

    Elaborator elaborator;
    ElaboratedModule em = elaborator.elaborate(flat);
    CombEvaluator eval(em);

    CHECK_EQ(eval.eval({{"a", 1}, {"b", 1}}).at("y"), 1u);
    CHECK_EQ(eval.eval({{"a", 1}, {"b", 0}}).at("y"), 0u);
}

static void test_sequential_hierarchy_shift_register() {
    std::string src = R"(
        module dff(input clk, input rst, input d, output q);
            reg q;
            always @(posedge clk)
                if (rst) q <= 0;
                else q <= d;
        endmodule

        module shiftreg2(input clk, input rst, input d, output q0, output q1);
            dff u0(.clk(clk), .rst(rst), .d(d), .q(q0));
            dff u1(.clk(clk), .rst(rst), .d(q0), .q(q1));
        endmodule
    )";
    Design design = parse_design(src);
    Flattener flattener(design);
    Module flat = flattener.flatten("shiftreg2");

    Elaborator elaborator;
    ElaboratedModule em = elaborator.elaborate(flat);
    SeqEvaluator sim(em);

    sim.step({{"clk", 0}, {"rst", 1}, {"d", 0}});
    sim.step({{"clk", 1}, {"rst", 1}, {"d", 0}}); // posedge: reset both stages
    CHECK_EQ(sim.reg_value("u0__q"), 0u);
    CHECK_EQ(sim.reg_value("u1__q"), 0u);

    sim.step({{"clk", 0}, {"rst", 0}, {"d", 1}});
    sim.step({{"clk", 1}, {"rst", 0}, {"d", 1}}); // posedge: q0 <= 1, q1 <= old q0 (0)
    CHECK_EQ(sim.reg_value("u0__q"), 1u);
    CHECK_EQ(sim.reg_value("u1__q"), 0u);

    sim.step({{"clk", 0}, {"rst", 0}, {"d", 0}});
    sim.step({{"clk", 1}, {"rst", 0}, {"d", 0}}); // posedge: q0 <= 0, q1 <= old q0 (1)
    CHECK_EQ(sim.reg_value("u0__q"), 0u);
    CHECK_EQ(sim.reg_value("u1__q"), 1u);
}

static void test_missing_module_throws() {
    std::string src = R"(
        module top(input a, output y);
            missing_module u(.a(a), .y(y));
        endmodule
    )";
    Design design = parse_design(src);
    Flattener flattener(design);
    bool threw = false;
    try {
        flattener.flatten("top");
    } catch (const FlattenError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_two_instance_combinational_hierarchy();
    test_parameter_overrides_per_instance();
    test_nested_two_level_hierarchy();
    test_positional_connections();
    test_sequential_hierarchy_shift_register();
    test_missing_module_throws();
    TEST_MAIN_RETURN();
}
