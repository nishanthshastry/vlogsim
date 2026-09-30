# vlogsim

A mini event-driven RTL simulator for a subset of Verilog, built in C++.
One netlist IR is meant to eventually drive three interchangeable
simulation engines (event-driven interpreter, levelized evaluator, and
generated-C++ execution), validated against Icarus Verilog as the golden
reference.

This repo currently implements **Milestone 1** of the roadmap below:
lexer, parser, elaborator, and a combinational `assign` evaluator, with
unit tests on an adder and a mux.

## Scope (Verilog subset)

Modules and ports, `wire`/`reg`, continuous `assign`, `always @(posedge
clk)` and `always @*`, blocking/non-blocking assignment, `if`/`case`,
arithmetic/bitwise/logical/shift/reduction operators, parameters, and
hierarchy through module instantiation. Milestone 1 covers the
combinational slice of this: modules, ports, wire/reg declarations, and
`assign` with full expression precedence (no bit-select or hierarchy yet).

## Build & test

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Run the CLI on an example design (parses, elaborates, and prints net
widths):

```bash
./vlogsim_cli ../examples/mux2to1.v
```

## Roadmap

1. **Combinational subset** — lexer, parser, elaborator, `assign`
   evaluation; unit tests on adders and multiplexers. *(this repo, so far)*
2. Sequential logic — clocked `always` blocks, non-blocking assignment,
   reset, VCD output.
3. Hierarchy and parameters — module instantiation and flattening.
4. Four-state logic — X propagation and pessimism tests.
5. Testbench support — `initial` blocks, delays, `$display`, `$finish`.
6. Performance — levelized engine, then C++ code generation, benchmarked
   against the interpreter.

## Validation

- **Correctness**: cycle-by-cycle VCD comparison against Icarus Verilog
  under randomized stimulus (target: zero mismatches).
- **Performance**: simulated cycles/second per engine, and speedup of the
  levelized/generated-code engines over the interpreter.
- **Engineering quality**: parser unit tests, ASan/UBSan runs, CI.

## AI extension (planned)

LLM generate-and-verify loop: an LLM writes Verilog from a spec, this
simulator runs the tests, and failing traces are fed back for repair.
Compared against single-shot generation with no feedback; success metric
is pass rate after *k* repair rounds.

## Reference tools

[Verilator](https://www.veripool.org/verilator/),
[Icarus Verilog](http://iverilog.icarus.com/),
[GTKWave](http://gtkwave.sourceforge.net/).
