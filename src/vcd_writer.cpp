#include "vlogsim/vcd_writer.h"

#include <bitset>
#include <stdexcept>

namespace vlogsim {

// VCD identifiers are arbitrary strings of printable ASCII 33 ('!') to 126
// ('~'); this generates them in order like a base-94 counter, which is
// plenty for any design this simulator will realistically handle.
std::string VcdWriter::make_id(size_t index) {
    static const int kFirst = '!';
    static const int kRange = '~' - '!' + 1;
    std::string id;
    do {
        id.push_back(static_cast<char>(kFirst + (index % kRange)));
        index /= kRange;
    } while (index > 0);
    return id;
}

void VcdWriter::add_signal(const std::string& name, int width) {
    if (signals_.count(name)) throw std::runtime_error("VCD signal '" + name + "' added more than once");
    SignalInfo info;
    info.id = make_id(signal_order_.size());
    info.width = width;
    signals_[name] = info;
    signal_order_.push_back(name);
}

void VcdWriter::write_header(const std::string& scope_name, const std::string& timescale) {
    out_ << "$timescale " << timescale << " $end\n";
    out_ << "$scope module " << scope_name << " $end\n";
    for (const std::string& name : signal_order_) {
        const SignalInfo& info = signals_.at(name);
        out_ << "$var wire " << info.width << " " << info.id << " " << name << " $end\n";
    }
    out_ << "$upscope $end\n";
    out_ << "$enddefinitions $end\n";
}

namespace {
std::string format_value(uint64_t value, int width, const std::string& id) {
    if (width == 1) {
        return std::string(1, value != 0 ? '1' : '0') + id;
    }
    std::string bits;
    for (int i = width - 1; i >= 0; --i) {
        bits.push_back(((value >> i) & 1u) ? '1' : '0');
    }
    return "b" + bits + " " + id;
}
} // namespace

void VcdWriter::dump_step(uint64_t time, const std::unordered_map<std::string, uint64_t>& values) {
    out_ << "#" << time << "\n";
    bool is_first_dump = true;
    for (const std::string& name : signal_order_) {
        if (signals_.at(name).has_value) { is_first_dump = false; break; }
    }

    if (is_first_dump) out_ << "$dumpvars\n";

    for (const std::string& name : signal_order_) {
        SignalInfo& info = signals_.at(name);
        auto it = values.find(name);
        if (it == values.end()) continue; // signal not driven this step; VCD keeps last value implicitly
        if (info.has_value && it->second == info.last_value) continue;

        out_ << format_value(it->second, info.width, info.id) << "\n";
        info.has_value = true;
        info.last_value = it->second;
    }

    if (is_first_dump) out_ << "$end\n";
}

} // namespace vlogsim
