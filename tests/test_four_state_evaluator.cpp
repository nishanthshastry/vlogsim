#include "vlogsim/elaborator.h"
#include "vlogsim/four_state_evaluator.h"
#include "vlogsim/lexer.h"
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

static FourState X1() { return FourState{0, 1}; }

static void test_and_gate_absorbs_known_zero() {
    const char* src = R"(
        module and_gate(input a, input b, output y);
            assign y = a & b;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    FourStateCombEvaluator eval(em);

    // One known-0 input decides the AND regardless of the other being X —
    // the non-pessimistic absorbing case.
    auto r1 = eval.eval({{"a", X1()}, {"b", four_known(0)}});
    CHECK(r1.at("y") == four_known(0));

    // No known-0 input: result is X.
    auto r2 = eval.eval({{"a", X1()}, {"b", four_known(1)}});
    CHECK(r2.at("y") == X1());
}

static void test_mux_merges_agreeing_branches() {
    const char* src = R"(
        module mux2to1(input a, input b, input sel, output out);
            assign out = sel ? b : a;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    FourStateCombEvaluator eval(em);

    // sel is X, but both branches agree (a == b == 1): result is known 1,
    // not X — this is the key non-pessimistic rule for the ternary operator.
    auto agree = eval.eval({{"a", four_known(1)}, {"b", four_known(1)}, {"sel", X1()}});
    CHECK(agree.at("out") == four_known(1));

    // sel is X and branches disagree: result is X.
    auto disagree = eval.eval({{"a", four_known(1)}, {"b", four_known(0)}, {"sel", X1()}});
    CHECK(disagree.at("out") == X1());

    // sel is known: ordinary selection, ignoring the other branch's value.
    auto known_sel = eval.eval({{"a", four_known(1)}, {"b", X1()}, {"sel", four_known(0)}});
    CHECK(known_sel.at("out") == four_known(1));
}

static void test_literal_with_x_digits_parses_correctly() {
    const char* src = R"(
        module lit_test(output [3:0] y);
            assign y = 4'bxx01;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    FourStateCombEvaluator eval(em);

    auto result = eval.eval({});
    FourState y = result.at("y");
    CHECK_EQ(y.bits, 0b0001u);
    CHECK_EQ(y.unknown, 0b1100u); // top two bits are the 'x' digits
}

int main() {
    test_and_gate_absorbs_known_zero();
    test_mux_merges_agreeing_branches();
    test_literal_with_x_digits_parses_correctly();
    TEST_MAIN_RETURN();
}
