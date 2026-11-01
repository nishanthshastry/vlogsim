// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "mini_test.h"

using namespace vlogsim;

static const char* kMuxSrc = R"(
    module mux2to1(
        input a,
        input b,
        input sel,
        output out
    );
        assign out = sel ? b : a;
    endmodule
)";

// 4-to-1 mux built from nested ternaries, to exercise a slightly deeper
// combinational dependency chain than the 2:1 case.
static const char* kMux4Src = R"(
    module mux4to1(
        input [1:0] sel,
        input a,
        input b,
        input c,
        input d,
        output out
    );
        wire lo;
        wire hi;
        assign lo = sel[0] ? b : a;
        assign hi = sel[0] ? d : c;
        assign out = sel[1] ? hi : lo;
    endmodule
)";

static ElaboratedModule elaborate_source(const char* src, Module& module_storage) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    module_storage = parser.parse_module();
    Elaborator elaborator;
    return elaborator.elaborate(module_storage);
}

static void test_mux2to1_truth_table() {
    Module module;
    ElaboratedModule em = elaborate_source(kMuxSrc, module);
    CombEvaluator eval(em);

    struct Case { uint64_t a, b, sel, expected; };
    std::vector<Case> cases = {
        {0, 1, 0, 0}, // sel=0 selects a
        {0, 1, 1, 1}, // sel=1 selects b
        {1, 0, 0, 1},
        {1, 0, 1, 0},
    };
    for (const auto& c : cases) {
        auto result = eval.eval({{"a", c.a}, {"b", c.b}, {"sel", c.sel}});
        CHECK_EQ(result.at("out"), c.expected);
    }
}

int main() {
    test_mux2to1_truth_table();
    // kMux4Src exercises bit-select (sel[0]/sel[1]), which is out of the
    // Milestone-1 grammar; kept here as a documented follow-up once
    // bit-select parsing lands, rather than silently skipped.
    (void)kMux4Src;
    TEST_MAIN_RETURN();
}
