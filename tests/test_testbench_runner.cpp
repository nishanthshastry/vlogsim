// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include <sstream>

#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "vlogsim/testbench_runner.h"
#include "mini_test.h"

using namespace vlogsim;

static ElaboratedModule elaborate_source(const char* src, Module& storage) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    storage = parser.parse_module();
    Elaborator elaborator;
    return elaborator.elaborate(storage);
}

static void test_delays_and_blocking_assignment() {
    const char* src = R"(
        module tb;
            reg [7:0] a;
            reg [7:0] sum;
            initial begin
                a = 1;
                sum = 0;
                #1 sum = sum + a;
                a = 2;
                #1 sum = sum + a;
                a = 3;
                #1 sum = sum + a;
                $finish;
            end
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    std::ostringstream out;
    TestbenchRunner tb(em, out);
    tb.run();

    CHECK(tb.finished());
    CHECK_EQ(tb.time(), 3u);
    CHECK_EQ(tb.value_of("sum"), 6u); // 1 + 2 + 3
}

static void test_display_formats_and_time() {
    const char* src = R"(
        module tb;
            reg [3:0] x;
            initial begin
                x = 5;
                $display("x=%d", x);
                #2;
                x = 10;
                $display("t=%d x=%h", $time, x);
                $finish;
            end
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    std::ostringstream out;
    TestbenchRunner tb(em, out);
    tb.run();

    std::string text = out.str();
    CHECK(text.find("x=5") != std::string::npos);
    CHECK(text.find("t=2 x=a") != std::string::npos); // 10 decimal is 'a' in hex
}

static void test_finish_stops_execution_immediately() {
    const char* src = R"(
        module tb;
            reg done;
            initial begin
                done = 0;
                $finish;
                done = 1;
            end
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    std::ostringstream out;
    TestbenchRunner tb(em, out);
    tb.run();

    CHECK(tb.finished());
    CHECK_EQ(tb.value_of("done"), 0u); // the statement after $finish never ran
}

static void test_bare_delay_statement() {
    const char* src = R"(
        module tb;
            reg [3:0] v;
            initial begin
                v = 1;
                #5;
                v = 2;
            end
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    std::ostringstream out;
    TestbenchRunner tb(em, out);
    tb.run();

    CHECK_EQ(tb.time(), 5u);
    CHECK_EQ(tb.value_of("v"), 2u);
    CHECK(!tb.finished()); // no $finish in this testbench
}

static void test_if_else_inside_initial_block() {
    const char* src = R"(
        module tb;
            reg [3:0] a;
            reg [3:0] max_val;
            initial begin
                a = 7;
                if (a > 3)
                    max_val = a;
                else
                    max_val = 3;
            end
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    std::ostringstream out;
    TestbenchRunner tb(em, out);
    tb.run();

    CHECK_EQ(tb.value_of("max_val"), 7u);
}

int main() {
    test_delays_and_blocking_assignment();
    test_display_formats_and_time();
    test_finish_stops_execution_immediately();
    test_bare_delay_statement();
    test_if_else_inside_initial_block();
    TEST_MAIN_RETURN();
}
