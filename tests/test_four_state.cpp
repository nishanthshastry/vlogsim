#include "vlogsim/value.h"
#include "mini_test.h"

using namespace vlogsim;

static FourState X(int width = 1) { return FourState{0, mask_for_width(width)}; }
static FourState K(uint64_t v) { return four_known(v); }

static void test_and_absorbing_zero() {
    // 0 & X = 0, known — the absorbing-element rule, not pessimistic.
    CHECK(four_and(K(0), X(), 1) == K(0));
    CHECK(four_and(X(), K(0), 1) == K(0));
    // 1 & X = X.
    CHECK(four_and(K(1), X(), 1) == X());
    // X & X = X.
    CHECK(four_and(X(), X(), 1) == X());
    CHECK(four_and(K(1), K(1), 1) == K(1));
}

static void test_or_absorbing_one() {
    CHECK(four_or(K(1), X(), 1) == K(1));
    CHECK(four_or(X(), K(1), 1) == K(1));
    CHECK(four_or(K(0), X(), 1) == X());
    CHECK(four_or(X(), X(), 1) == X());
    CHECK(four_or(K(0), K(0), 1) == K(0));
}

static void test_xor_no_absorbing_case() {
    // XOR has no absorbing input value: any unknown bit makes the result X.
    CHECK(four_xor(K(0), X(), 1) == X());
    CHECK(four_xor(K(1), X(), 1) == X());
    CHECK(four_xor(K(1), K(0), 1) == K(1));
    CHECK(four_xor(K(1), K(1), 1) == K(0));
}

static void test_not() {
    CHECK(four_not(K(0), 1) == K(1));
    CHECK(four_not(K(1), 1) == K(0));
    CHECK(four_not(X(), 1) == X()); // ~X = X
}

static void test_equality() {
    CHECK(four_eq(K(5), K(5), 4) == K(1));
    CHECK(four_eq(K(5), K(6), 4) == K(0));
    CHECK(four_eq(K(5), X(4), 4) == X());
    CHECK(four_eq(X(4), X(4), 4) == X()); // X == X is unknown, not true
}

static void test_reductions() {
    CHECK(four_reduce_and(K(0b1111), 4) == K(1));
    CHECK(four_reduce_and(K(0b1110), 4) == K(0)); // a known-0 bit decides false even with other bits
    CHECK(four_reduce_or(K(0b0000), 4) == K(0));
    CHECK(four_reduce_or(K(0b0001), 4) == K(1));

    // All-known-1 bits except one X: AND can't be decided true (needs every
    // bit =1) but no bit is known-0 either, so it's unknown, not pessimistic
    // "always false".
    FourState mostly_ones{0b0111, 0b1000}; // bits 0-2 known 1, bit 3 unknown
    CHECK(four_reduce_and(mostly_ones, 4) == X());
    // OR with one known-1 bit present is decided true regardless of X elsewhere.
    FourState one_known_one{0b0001, 0b1110};
    CHECK(four_reduce_or(one_known_one, 4) == K(1));
}

static void test_ternary_merge_on_unknown_condition() {
    // Branches agree on every bit: result is that known value, even
    // though the condition itself is X — the non-pessimistic case.
    CHECK(four_merge_on_unknown_cond(K(0b1010), K(0b1010), 4) == K(0b1010));
    // Branches disagree on a bit: that bit (and only that bit) goes X.
    FourState merged = four_merge_on_unknown_cond(K(0b1010), K(0b1000), 4);
    CHECK_EQ(merged.bits, 0b1000u);
    CHECK_EQ(merged.unknown, 0b0010u); // only the differing bit is X
}

static void test_classic_pessimism_a_or_not_a() {
    // In real hardware, a | ~a is always 1. A standard (non-optimistic)
    // four-state simulator reports X here instead, because it propagates
    // unknowns structurally rather than proving the tautology — this is
    // the expected, documented pessimism the design doc's risk table asks
    // Milestone 4 to have tests for, not a bug.
    FourState a = X();
    FourState result = four_or(a, four_not(a, 1), 1);
    CHECK(result == X());
}

static void test_mask_four_state_clears_high_bits() {
    FourState v{0xFF, 0xFF};
    FourState masked = mask_four_state(v, 4);
    CHECK_EQ(masked.bits & ~0xFull, 0u);
    CHECK_EQ(masked.unknown & ~0xFull, 0u);
}

int main() {
    test_and_absorbing_zero();
    test_or_absorbing_one();
    test_xor_no_absorbing_case();
    test_not();
    test_equality();
    test_reductions();
    test_ternary_merge_on_unknown_condition();
    test_classic_pessimism_a_or_not_a();
    test_mask_four_state_clears_high_bits();
    TEST_MAIN_RETURN();
}
