#pragma once

#include <cstdint>

namespace vlogsim {

// Milestone-1/2/3 evaluators (CombEvaluator, SeqEvaluator) use this
// two-state representation, capped at 64 bits.
inline uint64_t mask_for_width(int width) {
    if (width <= 0) return 0;
    if (width >= 64) return ~uint64_t{0};
    return (uint64_t{1} << width) - 1;
}

inline uint64_t mask_to_width(uint64_t value, int width) {
    return value & mask_for_width(width);
}

// --- Milestone 4: four-state values and X-propagation ---
//
// A bit is "unknown" (X) when its `unknown` bit is 1; `bits` is only
// meaningful where `unknown` is 0 (kept canonically 0 there otherwise).
// This project does not model multi-driver resolution, so 'z' literal
// digits are treated identically to 'x' — a documented simplification,
// not a distinct high-impedance state.
struct FourState {
    uint64_t bits = 0;
    uint64_t unknown = 0;

    bool operator==(const FourState& o) const { return bits == o.bits && unknown == o.unknown; }
};

inline FourState four_known(uint64_t v) { return FourState{v, 0}; }

inline FourState mask_four_state(FourState v, int width) {
    uint64_t m = mask_for_width(width);
    v.unknown &= m;
    v.bits &= m & ~v.unknown; // canonical form: bits is 0 wherever unknown is 1
    return v;
}

enum class TriBool { False, True, Unknown };

// Logical truthiness of a (possibly multi-bit) four-state value: true if
// any bit is known-1, false if every bit (within width) is known-0,
// otherwise unknown — matching Verilog's `if`/`&&`/`||` condition rules.
inline TriBool four_truth(FourState v, int width) {
    v = mask_four_state(v, width);
    if (v.bits != 0) return TriBool::True;
    if (v.unknown == 0) return TriBool::False;
    return TriBool::Unknown;
}

inline FourState four_from_tribool(TriBool t) {
    switch (t) {
        case TriBool::True: return FourState{1, 0};
        case TriBool::False: return FourState{0, 0};
        default: return FourState{0, 1};
    }
}

// Bitwise AND/OR/XOR/NOT with standard four-state propagation: AND/OR are
// absorbing (a known 0 forces AND to 0 regardless of the other operand's
// unknowns; a known 1 forces OR to 1 the same way); XOR and arithmetic
// have no absorbing element, so any unknown input bit makes that output
// bit unknown.
inline FourState four_and(FourState a, FourState b, int width) {
    a = mask_four_state(a, width);
    b = mask_four_state(b, width);
    uint64_t a_known0 = ~a.unknown & ~a.bits;
    uint64_t b_known0 = ~b.unknown & ~b.bits;
    uint64_t a_known1 = ~a.unknown & a.bits;
    uint64_t b_known1 = ~b.unknown & b.bits;
    uint64_t known0 = (a_known0 | b_known0) & mask_for_width(width);
    uint64_t known1 = (a_known1 & b_known1) & mask_for_width(width);
    return FourState{known1, mask_for_width(width) & ~(known0 | known1)};
}

inline FourState four_or(FourState a, FourState b, int width) {
    a = mask_four_state(a, width);
    b = mask_four_state(b, width);
    uint64_t a_known1 = ~a.unknown & a.bits;
    uint64_t b_known1 = ~b.unknown & b.bits;
    uint64_t a_known0 = ~a.unknown & ~a.bits;
    uint64_t b_known0 = ~b.unknown & ~b.bits;
    uint64_t known1 = (a_known1 | b_known1) & mask_for_width(width);
    uint64_t known0 = (a_known0 & b_known0) & mask_for_width(width);
    return FourState{known1, mask_for_width(width) & ~(known0 | known1)};
}

inline FourState four_xor(FourState a, FourState b, int width) {
    a = mask_four_state(a, width);
    b = mask_four_state(b, width);
    uint64_t known_both = ~a.unknown & ~b.unknown & mask_for_width(width);
    uint64_t value = (a.bits ^ b.bits) & known_both;
    return FourState{value, mask_for_width(width) & ~known_both};
}

inline FourState four_not(FourState a, int width) {
    a = mask_four_state(a, width);
    uint64_t m = mask_for_width(width);
    uint64_t value = ~a.bits & ~a.unknown & m;
    return FourState{value, a.unknown};
}

// Arithmetic/shift/relational ops have no useful absorbing cases in this
// subset, so any unknown input bit makes the whole result unknown — the
// simplification real toy simulators commonly make rather than modeling
// per-bit carry propagation through unknowns.
inline bool four_has_unknown(FourState v, int width) { return mask_four_state(v, width).unknown != 0; }

inline FourState four_full_x(int width) { return FourState{0, mask_for_width(width)}; }

// Four-state equality/inequality (the `==`/`!=` operators, not `===`):
// X in either operand makes the comparison's result X.
inline FourState four_eq(FourState a, FourState b, int width) {
    if (four_has_unknown(a, width) || four_has_unknown(b, width)) return four_from_tribool(TriBool::Unknown);
    uint64_t m = mask_for_width(width);
    return four_from_tribool((a.bits & m) == (b.bits & m) ? TriBool::True : TriBool::False);
}

// Bitwise reductions (unary &, |, ^): & is true only if every bit is
// known-1; | is true if any bit is known-1; both are unknown only when no
// deciding (known 0 for &, known 1 for |) bit exists yet some bit is X.
inline FourState four_reduce_and(FourState a, int width) {
    a = mask_four_state(a, width);
    uint64_t m = mask_for_width(width);
    if ((~a.unknown & ~a.bits & m) != 0) return four_from_tribool(TriBool::False);
    if (a.unknown != 0) return four_from_tribool(TriBool::Unknown);
    return four_from_tribool(TriBool::True);
}

inline FourState four_reduce_or(FourState a, int width) {
    a = mask_four_state(a, width);
    uint64_t m = mask_for_width(width);
    if ((~a.unknown & a.bits & m) != 0) return four_from_tribool(TriBool::True);
    if (a.unknown != 0) return four_from_tribool(TriBool::Unknown);
    return four_from_tribool(TriBool::False);
}

inline FourState four_reduce_xor(FourState a, int width) {
    a = mask_four_state(a, width);
    if (a.unknown != 0) return four_from_tribool(TriBool::Unknown);
    return four_from_tribool(__builtin_parityll(a.bits) ? TriBool::True : TriBool::False);
}

// The ternary operator's X-condition rule: when the condition is
// unknown, Verilog merges the two branches bit by bit, keeping a bit's
// value only where both branches agree on a known value, and setting it
// to X everywhere else — not simply picking one branch.
inline FourState four_merge_on_unknown_cond(FourState then_v, FourState else_v, int width) {
    then_v = mask_four_state(then_v, width);
    else_v = mask_four_state(else_v, width);
    uint64_t then_known0 = ~then_v.unknown & ~then_v.bits;
    uint64_t then_known1 = ~then_v.unknown & then_v.bits;
    uint64_t else_known0 = ~else_v.unknown & ~else_v.bits;
    uint64_t else_known1 = ~else_v.unknown & else_v.bits;
    uint64_t m = mask_for_width(width);
    uint64_t agree0 = then_known0 & else_known0 & m;
    uint64_t agree1 = then_known1 & else_known1 & m;
    return FourState{agree1, m & ~(agree0 | agree1)};
}

// Concatenation combines the value and unknown fields the same way plain
// two-state concatenation combines values: shift each part into place.
inline FourState four_concat_part(FourState acc, FourState part, int part_width, int shift) {
    FourState p = mask_four_state(part, part_width);
    acc.bits |= (p.bits << shift);
    acc.unknown |= (p.unknown << shift);
    return acc;
}

} // namespace vlogsim
