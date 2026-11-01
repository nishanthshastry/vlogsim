// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/levelized_evaluator.h"
#include "vlogsim/parser.h"
#include "mini_test.h"

using namespace vlogsim;

static ElaboratedModule elaborate_source(const char* src, Module& storage) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    storage = parser.parse_module();
    Elaborator elaborator;
    return elaborator.elaborate(storage);
}

static void test_matches_comb_evaluator_on_adder() {
    const char* src = R"(
        module adder4(
            input [3:0] a,
            input [3:0] b,
            input cin,
            output [4:0] sum
        );
            assign sum = a + b + cin;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    CombEvaluator interp(em);
    LevelizedEvaluator levelized(em);

    for (uint64_t a = 0; a < 16; a += 3) {
        for (uint64_t b = 0; b < 16; b += 5) {
            for (uint64_t cin = 0; cin <= 1; ++cin) {
                std::unordered_map<std::string, uint64_t> inputs = {{"a", a}, {"b", b}, {"cin", cin}};
                auto expected = interp.eval(inputs);
                auto actual = levelized.eval(inputs);
                CHECK_EQ(actual.at("sum"), expected.at("sum"));
            }
        }
    }
}

static void test_multi_level_intermediate_wires() {
    // Exercises genuine topological ordering: `y` depends on `mid`, which
    // depends on `a`/`b` — order must resolve `mid` before `y`.
    const char* src = R"(
        module chain(input a, input b, input c, output y);
            wire mid;
            assign mid = a & b;
            assign y = mid | c;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    CombEvaluator interp(em);
    LevelizedEvaluator levelized(em);

    for (uint64_t a = 0; a <= 1; ++a) {
        for (uint64_t b = 0; b <= 1; ++b) {
            for (uint64_t c = 0; c <= 1; ++c) {
                std::unordered_map<std::string, uint64_t> inputs = {{"a", a}, {"b", b}, {"c", c}};
                auto expected = interp.eval(inputs);
                auto actual = levelized.eval(inputs);
                CHECK_EQ(actual.at("y"), expected.at("y"));
                CHECK_EQ(actual.at("mid"), expected.at("mid"));
            }
        }
    }
}

static void test_reversed_declaration_order_still_resolves() {
    // `y`'s assign appears before `mid`'s in source order, but the
    // topological sort must still put `mid` first since `y` depends on it.
    const char* src = R"(
        module reversed(input a, input b, output y);
            wire mid;
            assign y = mid;
            assign mid = a & b;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    LevelizedEvaluator levelized(em);

    CHECK_EQ(levelized.eval({{"a", 1}, {"b", 1}}).at("y"), 1u);
    CHECK_EQ(levelized.eval({{"a", 1}, {"b", 0}}).at("y"), 0u);
}

static void test_combinational_cycle_detected_at_construction() {
    const char* src = R"(
        module cyclic(input a, output y);
            wire t;
            assign y = t;
            assign t = y;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    bool threw = false;
    try {
        LevelizedEvaluator levelized(em);
        (void)levelized;
    } catch (const LevelizedEvalError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_matches_comb_evaluator_on_adder();
    test_multi_level_intermediate_wires();
    test_reversed_declaration_order_still_resolves();
    test_combinational_cycle_detected_at_construction();
    TEST_MAIN_RETURN();
}
