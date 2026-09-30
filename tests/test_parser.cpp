#include "vlogsim/lexer.h"
#include "vlogsim/parser.h"
#include "mini_test.h"

using namespace vlogsim;

static Module parse(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    return parser.parse_module();
}

static void test_ports_and_widths() {
    Module m = parse(R"(
        module foo;
            input [3:0] a;
            input b;
            output [7:0] y;
        endmodule
    )");
    CHECK_EQ(m.name, std::string("foo"));
    CHECK_EQ(m.ports.size(), 3u);
    CHECK_EQ(m.ports[0].name, std::string("a"));
    CHECK(m.ports[0].direction == PortDirection::Input);
    CHECK_EQ(m.ports[0].msb, 3);
    CHECK_EQ(m.ports[0].lsb, 0);
    CHECK_EQ(m.ports[1].name, std::string("b"));
    CHECK_EQ(m.ports[2].name, std::string("y"));
    CHECK(m.ports[2].direction == PortDirection::Output);
    CHECK_EQ(m.ports[2].msb, 7);
}

static void test_module_header_port_list_is_skipped() {
    Module m = parse(R"(
        module bar(a, b, y);
            input a;
            input b;
            output y;
            assign y = a & b;
        endmodule
    )");
    CHECK_EQ(m.name, std::string("bar"));
    CHECK_EQ(m.ports.size(), 3u);
    CHECK_EQ(m.assigns.size(), 1u);
}

static void test_assign_expression_tree() {
    Module m = parse(R"(
        module baz;
            input a;
            input b;
            input sel;
            output out;
            assign out = sel ? b : a;
        endmodule
    )");
    CHECK_EQ(m.assigns.size(), 1u);
    CHECK_EQ(m.assigns[0].target, std::string("out"));
    auto* tern = dynamic_cast<TernaryExpr*>(m.assigns[0].rhs.get());
    CHECK(tern != nullptr);
    auto* cond = dynamic_cast<IdentExpr*>(tern->cond.get());
    CHECK(cond != nullptr);
    CHECK_EQ(cond->name, std::string("sel"));
}

static void test_operator_precedence() {
    // a + b & c should parse as (a + b) & c, since & binds looser than +.
    Module m = parse(R"(
        module p;
            input a, b, c;
            output y;
            assign y = a + b & c;
        endmodule
    )");
    auto* top = dynamic_cast<BinaryExpr*>(m.assigns[0].rhs.get());
    CHECK(top != nullptr);
    CHECK(top->op == BinOp::And);
    auto* lhs = dynamic_cast<BinaryExpr*>(top->lhs.get());
    CHECK(lhs != nullptr);
    CHECK(lhs->op == BinOp::Add);
}

static void test_parse_error_on_bad_syntax() {
    bool threw = false;
    try {
        parse("module m; input; endmodule");
    } catch (const ParseError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_ports_and_widths();
    test_module_header_port_list_is_skipped();
    test_assign_expression_tree();
    test_operator_precedence();
    test_parse_error_on_bad_syntax();
    TEST_MAIN_RETURN();
}
