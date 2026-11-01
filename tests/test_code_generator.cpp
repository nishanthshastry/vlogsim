// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "vlogsim/code_generator.h"
#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "mini_test.h"

using namespace vlogsim;

// VLOGSIM_CXX_COMPILER and VLOGSIM_CXX_STD_FLAG are injected by CMake so
// this test compiles generated code with the exact compiler/standard the
// project itself was configured with, rather than guessing a "c++" on PATH.
#ifndef VLOGSIM_CXX_COMPILER
#define VLOGSIM_CXX_COMPILER "c++"
#endif
#ifndef VLOGSIM_CXX_STD_FLAG
#define VLOGSIM_CXX_STD_FLAG "-std=c++17"
#endif

static ElaboratedModule elaborate_source(const char* src, Module& storage) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    storage = parser.parse_module();
    Elaborator elaborator;
    return elaborator.elaborate(storage);
}

// Generates C++ for a small combinational module, compiles it with the
// system compiler into a standalone executable (Verilator-style), runs
// it, and checks its output against CombEvaluator for every input
// combination — proving codegen produces results identical to the
// interpreter, not just that it emits plausible-looking text.
static void test_generated_code_matches_interpreter() {
    const char* src = R"(
        module chain3(input a, input b, input c, output y);
            wire ab;
            assign ab = a & b;
            assign y = ab | c;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);

    CodeGenerator codegen(em);
    std::string generated = codegen.generate("eval_chain3");

    std::string driver = R"(
#include <fstream>
int main() {
    std::ofstream out("vlogsim_codegen_results.txt");
    for (uint64_t a = 0; a <= 1; ++a)
        for (uint64_t b = 0; b <= 1; ++b)
            for (uint64_t c = 0; c <= 1; ++c) {
                auto result = eval_chain3({{"a", a}, {"b", b}, {"c", c}});
                out << a << "," << b << "," << c << "," << result.at("y") << "\n";
            }
    return 0;
}
)";

    const char* cpp_path = "vlogsim_codegen_test.cpp";
    const char* exe_path = "vlogsim_codegen_test_exe";
    {
        std::ofstream f(cpp_path);
        f << generated << driver;
    }

    std::ostringstream compile_cmd;
    compile_cmd << VLOGSIM_CXX_COMPILER << " " << VLOGSIM_CXX_STD_FLAG << " -O2 -o " << exe_path << " " << cpp_path;
    int compile_rc = std::system(compile_cmd.str().c_str());
    CHECK_EQ(compile_rc, 0);
    if (compile_rc != 0) return; // nothing further to check if it didn't even compile

    std::ostringstream run_cmd;
    run_cmd << "./" << exe_path;
    int run_rc = std::system(run_cmd.str().c_str());
    CHECK_EQ(run_rc, 0);

    std::ifstream results("vlogsim_codegen_results.txt");
    CHECK(results.good());

    CombEvaluator interp(em);
    std::string line;
    int lines_checked = 0;
    while (std::getline(results, line)) {
        if (line.empty()) continue;
        uint64_t a, b, c, y;
        char comma;
        std::istringstream ls(line);
        ls >> a >> comma >> b >> comma >> c >> comma >> y;

        auto expected = interp.eval({{"a", a}, {"b", b}, {"c", c}});
        CHECK_EQ(y, expected.at("y"));
        lines_checked++;
    }
    CHECK_EQ(lines_checked, 8); // 2^3 input combinations

    std::remove(cpp_path);
    std::remove(exe_path);
    std::remove("vlogsim_codegen_results.txt");
}

static void test_generated_source_contains_expected_structure() {
    const char* src = R"(
        module and_gate(input a, input b, output y);
            assign y = a & b;
        endmodule
    )";
    Module module;
    ElaboratedModule em = elaborate_source(src, module);
    CodeGenerator codegen(em);
    std::string generated = codegen.generate();

    CHECK(generated.find("eval_and_gate") != std::string::npos);
    CHECK(generated.find("#include <cstdint>") != std::string::npos);
    CHECK(generated.find("in.at(\"a\")") != std::string::npos);
    CHECK(generated.find("in.at(\"b\")") != std::string::npos);
}

int main() {
    test_generated_code_matches_interpreter();
    test_generated_source_contains_expected_structure();
    TEST_MAIN_RETURN();
}
