# vlogsim

A mini event-driven RTL simulator for a subset of Verilog, built in C++.
One netlist IR is meant to eventually drive three interchangeable
simulation engines (event-driven interpreter, levelized evaluator, and
generated-C++ execution), validated against Icarus Verilog as the golden
reference.

This repo currently implements **Milestones 1 through 4** of the roadmap
below: lexer, parser, elaborator, a combinational `assign` evaluator,
clocked `always` blocks with non-blocking assignment and a VCD writer,
module instantiation with parameters flattened to a single module before
elaboration, and now four-state (0/1/X) values with real X-propagation —
validated with unit tests on an adder, a mux, a D flip-flop with
synchronous reset, a wraparound counter, a hierarchical 2-bit shift
register built from two instantiated D flip-flops, and X-propagation
through an AND gate, a mux, and a literal with `x` digits.

## Scope (Verilog subset)

Modules and ports, `wire`/`reg`, continuous `assign`, `always
@(posedge/negedge ...)` with `if`/`else` and `begin`/`end` blocks,
blocking (`=`) and non-blocking (`<=`) assignment,
arithmetic/bitwise/logical/shift/reduction operators, `parameter`
declarations with per-instance `#(.NAME(value))` overrides, and module
instantiation (named `.port(expr)` or positional connections) across
multiple modules in one file. `case` and bit-select remain for later
milestones.

Milestone 1 covers the combinational slice (modules, ports, wire/reg
declarations, `assign` with full expression precedence). Milestone 2
adds clocked `always` blocks: edge-sensitive triggering, blocking vs.
non-blocking assignment semantics (two-phase sample-then-commit, per the
proposal's kernel sketch), and VCD output. Milestone 3 adds hierarchy:
`Flattener` inlines every instantiated submodule into the top module —
renaming nets with an `<instance>__` prefix, bridging port connections
with continuous assigns, and constant-folding parameters away entirely —
so `Elaborator` and both evaluators need no hierarchy-awareness at all.
Always-block bodies can reference continuous-assign-driven wires when
resolving a sensitivity edge (needed for clock/reset pass-through after
flattening) but not yet in general statement bodies; `always @*`, `case`,
and bit-select remain for later milestones.

Milestone 4 adds a `FourState{bits, unknown}` value type (`value.h`) with
real per-operator X-propagation — AND/OR are absorbing (a known 0/1 input
decides the result even if the other operand is X), XOR/arithmetic have
no absorbing case (any X input makes the whole result X), and the
ternary operator merges its branches bit-by-bit on an unknown condition
rather than picking one, matching Verilog's actual (non-pessimistic
where it can be) rule. Number literals now parse `x`/`z` digits into a
real unknown mask instead of collapsing them to 0 (`z` is treated as `x`
— this project has no multi-driver/tristate resolution). This ships as a
**new, separate `FourStateCombEvaluator`** rather than a migration of the
existing two-state `CombEvaluator`/`SeqEvaluator`: the design doc frames
four-state as an upgrade *path* from the two-state baseline, and
migrating `SeqEvaluator` (X on registers, reset-to-X-until-first-clock)
is left as a follow-up rather than rewriting every Milestone 1-3 test's
assertions in one pass.

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
./vlogsim_cli ../examples/counter.v
./vlogsim_cli ../examples/shiftreg2.v   # hierarchical: two instantiated dff modules
```

The CLI parses every module in the file and infers the top module as the
one definition nothing else instantiates (pass a single-module file to
sidestep the inference entirely).

## Roadmap

1. **Combinational subset** — lexer, parser, elaborator, `assign`
   evaluation; unit tests on adders and multiplexers. *(done)*
2. **Sequential logic** — clocked `always` blocks, non-blocking
   assignment, reset, VCD output. *(done — see `SeqEvaluator` and
   `VcdWriter`)*
3. **Hierarchy and parameters** — module instantiation and flattening.
   *(done — see `Flattener`)*
4. **Four-state logic** — X propagation and pessimism tests. *(done — see
   `value.h`'s `FourState` and `FourStateCombEvaluator`; `SeqEvaluator`
   migration to four-state is a follow-up)*
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
