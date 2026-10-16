#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "vlogsim/seq_evaluator.h"
#include "mini_test.h"

using namespace vlogsim;

static ElaboratedModule elaborate_source(const char* src, Module& storage) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    storage = parser.parse_module();
    Elaborator elaborator;
    return elaborator.elaborate(storage);
}

static const char* kDffSrc = R"(
    module dff(
        input clk,
        input rst,
        input d,
        output q
    );
        reg q;
        always @(posedge clk)
            if (rst)
                q <= 0;
            else
                q <= d;
    endmodule
)";

static void test_dff_with_sync_reset() {
    Module module;
    ElaboratedModule em = elaborate_source(kDffSrc, module);
    SeqEvaluator sim(em);

    // First step just establishes a baseline sample; no edge fires yet.
    sim.step({{"clk", 0}, {"rst", 1}, {"d", 1}});
    CHECK_EQ(sim.reg_value("q"), 0u);

    // Rising edge with rst=1: q must clear regardless of d.
    sim.step({{"clk", 1}, {"rst", 1}, {"d", 1}});
    CHECK_EQ(sim.reg_value("q"), 0u);

    // Falling edge: no change (only posedge is in the sensitivity list).
    sim.step({{"clk", 0}, {"rst", 0}, {"d", 1}});
    CHECK_EQ(sim.reg_value("q"), 0u);

    // Rising edge with rst=0, d=1: q captures d.
    sim.step({{"clk", 1}, {"rst", 0}, {"d", 1}});
    CHECK_EQ(sim.reg_value("q"), 1u);

    // Rising edge with d=0: q captures 0.
    sim.step({{"clk", 0}, {"rst", 0}, {"d", 0}});
    sim.step({{"clk", 1}, {"rst", 0}, {"d", 0}});
    CHECK_EQ(sim.reg_value("q"), 0u);
}

static const char* kCounterSrc = R"(
    module counter(
        input clk,
        input rst,
        output [3:0] count
    );
        reg [3:0] count;
        always @(posedge clk) begin
            if (rst)
                count <= 0;
            else
                count <= count + 1;
        end
    endmodule
)";

static void test_counter_increments_and_wraps() {
    Module module;
    ElaboratedModule em = elaborate_source(kCounterSrc, module);
    SeqEvaluator sim(em);

    sim.step({{"clk", 0}, {"rst", 1}});
    sim.step({{"clk", 1}, {"rst", 1}}); // posedge, reset
    CHECK_EQ(sim.reg_value("count"), 0u);

    uint64_t expected = 0;
    for (int i = 0; i < 20; ++i) {
        sim.step({{"clk", 0}, {"rst", 0}});
        sim.step({{"clk", 1}, {"rst", 0}}); // posedge, increments
        expected = (expected + 1) & 0xF; // 4-bit wraparound
        CHECK_EQ(sim.reg_value("count"), expected);
    }
}

int main() {
    test_dff_with_sync_reset();
    test_counter_increments_and_wraps();
    TEST_MAIN_RETURN();
}
