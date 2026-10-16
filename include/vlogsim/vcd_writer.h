#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vlogsim {

// Minimal Value Change Dump writer: enough of the format for GTKWave (and
// this project's own tests) to read back — $timescale/$var/$enddefinitions
// header, then #<time> markers with only-on-change value lines. Four-state
// (x/z) values arrive with Milestone 4; this writer is two-state only.
class VcdWriter {
public:
    explicit VcdWriter(std::ostream& out) : out_(out) {}

    // Call once per signal before write_header(). `name` is also used as
    // the VCD identifier's human-readable label.
    void add_signal(const std::string& name, int width);

    void write_header(const std::string& scope_name = "top", const std::string& timescale = "1ns");

    // Emits "#<time>" followed by a value line for every signal whose
    // value differs from what was last written for it (or has never been
    // written). Call once per simulated time step, after write_header().
    void dump_step(uint64_t time, const std::unordered_map<std::string, uint64_t>& values);

private:
    struct SignalInfo {
        std::string id;   // short VCD identifier, e.g. "!", "\"", "#"...
        int width = 1;
        bool has_value = false;
        uint64_t last_value = 0;
    };

    static std::string make_id(size_t index);

    std::ostream& out_;
    std::vector<std::string> signal_order_; // preserves add_signal() order
    std::unordered_map<std::string, SignalInfo> signals_;
};

} // namespace vlogsim
