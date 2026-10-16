#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "vlogsim/comb_evaluator.h"
#include "vlogsim/elaborator.h"
#include "vlogsim/flattener.h"
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

// Picks the top module as the one definition no other module instantiates
// — the usual convention when a design doesn't name its top explicitly.
std::string find_top_module(const vlogsim::Design& design) {
    if (design.size() == 1) return design.begin()->first;

    std::unordered_set<std::string> instantiated;
    for (const auto& [name, module] : design) {
        (void)name;
        for (const vlogsim::ModuleInstance& inst : module.instances) instantiated.insert(inst.module_type);
    }

    std::string top;
    int candidates = 0;
    for (const auto& [name, module] : design) {
        (void)module;
        if (!instantiated.count(name)) {
            top = name;
            candidates++;
        }
    }
    if (candidates != 1) {
        throw std::runtime_error("could not determine a unique top module (found " + std::to_string(candidates) +
                                  " candidates not instantiated by anything else); pass a single-module file instead");
    }
    return top;
}
} // namespace

// CLI covering Milestones 1-3: parses every module in the file, flattens
// hierarchy (module instantiation + parameters) down to the inferred top
// module, elaborates it, and prints its port list and net widths.
// Stimulus-driven evaluation is exercised directly by the unit tests for
// now; a real testbench front end (initial blocks, $display) arrives in
// Milestone 5.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <file.v>\n";
        return 1;
    }
    try {
        std::string source = read_file(argv[1]);
        vlogsim::Lexer lexer(source);
        vlogsim::Parser parser(lexer.tokenize());
        vlogsim::Design design = parser.parse_design();

        std::string top_name = find_top_module(design);
        vlogsim::Flattener flattener(design);
        vlogsim::Module flat = flattener.flatten(top_name);

        vlogsim::Elaborator elaborator;
        vlogsim::ElaboratedModule em = elaborator.elaborate(flat);

        std::cout << "module " << em.name() << " (top of " << design.size() << " module(s) in file)\n";
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
