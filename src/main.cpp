#include <fstream>
#include <iostream>
#include <sstream>

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"

namespace {
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
} // namespace

// Milestone-1 CLI: parses and elaborates a single-module Verilog-subset
// file and prints its port list and net widths. Stimulus-driven evaluation
// is exercised directly by the unit tests for now; a real testbench front
// end (initial blocks, $display) arrives in Milestone 5.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <file.v>\n";
        return 1;
    }
    try {
        std::string source = read_file(argv[1]);
        vlogsim::Lexer lexer(source);
        vlogsim::Parser parser(lexer.tokenize());
        vlogsim::Module module = parser.parse_module();

        vlogsim::Elaborator elaborator;
        vlogsim::ElaboratedModule em = elaborator.elaborate(module);

        std::cout << "module " << em.name() << "\n";
        for (const auto& [name, info] : em.nets()) {
            std::cout << "  " << name << " : width=" << info.width
                       << (info.is_input ? " input" : (info.is_output ? " output" : " internal")) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
