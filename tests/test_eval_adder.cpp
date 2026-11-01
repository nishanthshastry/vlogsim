// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "mini_test.h"

using namespace vlogsim;

// 4-bit adder with a carry-out folded into a 5-bit sum output (no
// part-select/bit-select syntax yet, so cout is not split out separately).
static const char* kAdderSrc = R"(
    module adder4(
        input [3:0] a,
        input [3:0] b,
        input cin,
        output [4:0] sum
    );
        assign sum = a + b + cin;
    endmodule
)";

static ElaboratedModule elaborate_source(const char* src, Module& module_storage) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    module_storage = parser.parse_module();
    Elaborator elaborator;
    return elaborator.elaborate(module_storage);
}

static void test_adder_truth_table() {
    Module module;
    ElaboratedModule em = elaborate_source(kAdderSrc, module);
    CombEvaluator eval(em);

    struct Case { uint64_t a, b, cin, expected_sum; };
    std::vector<Case> cases = {
        {0, 0, 0, 0},
        {3, 5, 0, 8},
        {15, 1, 0, 16},   // carry into bit 4
        {15, 15, 1, 31},  // max 4-bit operands plus carry-in, fits exactly in 5 bits
        {7, 8, 1, 16},
    };

    for (const auto& c : cases) {
        auto result = eval.eval({{"a", c.a}, {"b", c.b}, {"cin", c.cin}});
        CHECK_EQ(result.at("sum"), c.expected_sum);
    }
}

static void test_missing_input_throws() {
    Module module;
    ElaboratedModule em = elaborate_source(kAdderSrc, module);
    CombEvaluator eval(em);
    bool threw = false;
    try {
        eval.eval({{"a", 1}, {"b", 2}}); // cin missing
    } catch (const EvalError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_adder_truth_table();
    test_missing_input_throws();
    TEST_MAIN_RETURN();
}
