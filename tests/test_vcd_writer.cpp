// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include <sstream>

#include "vlogsim/vcd_writer.h"
#include "mini_test.h"

using namespace vlogsim;

static void test_header_and_dumpvars() {
    std::ostringstream out;
    VcdWriter vcd(out);
    vcd.add_signal("clk", 1);
    vcd.add_signal("count", 4);
    vcd.write_header("top");
    vcd.dump_step(0, {{"clk", 0}, {"count", 0}});

    std::string text = out.str();
    CHECK(text.find("$timescale 1ns $end") != std::string::npos);
    CHECK(text.find("$var wire 1 ") != std::string::npos);
    CHECK(text.find("clk $end") != std::string::npos);
    CHECK(text.find("$var wire 4 ") != std::string::npos);
    CHECK(text.find("count $end") != std::string::npos);
    CHECK(text.find("$enddefinitions $end") != std::string::npos);
    CHECK(text.find("$dumpvars") != std::string::npos);
    CHECK(text.find("#0") != std::string::npos);
    CHECK(text.find("b0000 ") != std::string::npos); // 4-bit 0 dumped in binary form
}

static void test_only_changed_values_are_emitted() {
    std::ostringstream out;
    VcdWriter vcd(out);
    vcd.add_signal("a", 1);
    vcd.write_header();
    vcd.dump_step(0, {{"a", 0}});
    vcd.dump_step(10, {{"a", 0}}); // unchanged: should not add another value line
    vcd.dump_step(20, {{"a", 1}}); // changed: should add "1<id>"

    std::string text = out.str();
    size_t first_hash = text.find("#0");
    size_t second_hash = text.find("#10");
    size_t third_hash = text.find("#20");
    CHECK(first_hash != std::string::npos);
    CHECK(second_hash != std::string::npos);
    CHECK(third_hash != std::string::npos);

    // Between #10 and #20 there should be no value line at all (no change at t=10).
    std::string between = text.substr(second_hash, third_hash - second_hash);
    CHECK_EQ(between, std::string("#10\n"));

    CHECK(text.find("1!") != std::string::npos); // the changed value at t=20
}

int main() {
    test_header_and_dumpvars();
    test_only_changed_values_are_emitted();
    TEST_MAIN_RETURN();
}
