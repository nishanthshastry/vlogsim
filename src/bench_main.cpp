// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <iostream>
#include <sstream>

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/levelized_evaluator.h"
#include "vlogsim/parser.h"

namespace {

// Builds a module with a chain of N combinational stages (each depending
// on the previous one's wire), deep enough that per-call net-resolution
// overhead — the thing LevelizedEvaluator precomputes away — actually
// shows up in the numbers. A single AND/OR gate wouldn't.
std::string build_chain_source(int stages) {
    std::ostringstream src;
    src << "module chain(input a, input b, output y);\n";
    src << "    wire w0;\n";
    src << "    assign w0 = a ^ b;\n";
    for (int i = 1; i < stages; ++i) {
        src << "    wire w" << i << ";\n";
        src << "    assign w" << i << " = w" << (i - 1) << " ^ (a & b);\n";
    }
    src << "    assign y = w" << (stages - 1) << ";\n";
    src << "endmodule\n";
    return src.str();
}

template <typename EvalFn>
double bench(const std::string& label, int iterations, EvalFn&& eval_once) {
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) eval_once(i);
    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    double ops_per_sec = iterations / seconds;
    std::cout << label << ": " << iterations << " evals in " << seconds << "s => " << static_cast<long long>(ops_per_sec)
               << " evals/sec\n";
    return ops_per_sec;
}

} // namespace

// Not run under ctest: wall-clock benchmarks are inherently machine- and
// load-dependent, and a flaky timing-based CI test is worse than no CI
// test at all. Run manually: ./vlogsim_bench [stages] [iterations].
int main(int argc, char** argv) {
    int stages = argc > 1 ? std::stoi(argv[1]) : 64;
    int iterations = argc > 2 ? std::stoi(argv[2]) : 200000;

    std::string src = build_chain_source(stages);
    vlogsim::Lexer lexer(src);
    vlogsim::Parser parser(lexer.tokenize());
    vlogsim::Module module = parser.parse_module();
    vlogsim::Elaborator elaborator;
    vlogsim::ElaboratedModule em = elaborator.elaborate(module);

    vlogsim::CombEvaluator interp(em);
    vlogsim::LevelizedEvaluator levelized(em);

    std::cout << "chain depth: " << stages << " stages, " << iterations << " iterations each\n\n";

    double interp_ops = bench("CombEvaluator   (interpreter)", iterations, [&](int i) {
        interp.eval({{"a", static_cast<uint64_t>(i & 1)}, {"b", static_cast<uint64_t>((i >> 1) & 1)}});
    });
    double levelized_ops = bench("LevelizedEvaluator (levelized)", iterations, [&](int i) {
        levelized.eval({{"a", static_cast<uint64_t>(i & 1)}, {"b", static_cast<uint64_t>((i >> 1) & 1)}});
    });

    std::cout << "\nlevelized speedup over interpreter: " << (levelized_ops / interp_ops) << "x\n";
    return 0;
}
