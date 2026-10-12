#include "sn/app/telemetry.hpp"

#include <array>
#include <charconv>
#include <memory>

namespace sn::app {
namespace {

// Appends into a fixed buffer and remembers whether anything was cut off.
class Writer {
public:
    explicit Writer(std::span<char> out) : out_(out) {}

    Writer& text(std::string_view s) {
        if (!ok_ || s.size() >= out_.size() - used_) {
            ok_ = false;
            return *this;
        }
        for (const char c : s) {
            out_[used_++] = c;
        }
        return *this;
    }

    template <typename Int>
    Writer& number(Int value) {
        std::array<char, 24> digits{};
        char* const first = digits.data();
        const auto [end, ec] = std::to_chars(first, std::to_address(digits.end()), value);
        return text({first, static_cast<std::size_t>(end - first)});
    }

    Writer& hex8(std::uint32_t value) {
        constexpr std::string_view kHex = "0123456789abcdef";
        std::array<char, 8> digits{};
        for (std::size_t i = 0; i < digits.size(); ++i) {
            digits[digits.size() - 1 - i] = kHex[(value >> (4 * i)) & 0xFU];
        }
        return text({digits.data(), digits.size()});
    }

    Writer& boolean(bool value) { return text(value ? "true" : "false"); }

    std::size_t finish() {
        if (!ok_ || out_.empty()) {
            if (!out_.empty()) {
                out_[0] = '\0';
            }
            return 0;
        }
        out_[used_] = '\0';
        return used_;
    }

private:
    std::span<char> out_;
    std::size_t used_ = 0;
    bool ok_ = true;
};

}  // namespace

std::string_view outcomeName(GatewayTracker::Outcome outcome) {
    using O = GatewayTracker::Outcome;
    switch (outcome) {
        case O::NewNode: return "new";
        case O::InOrder: return "ok";
        case O::Gap: return "gap";
        case O::Duplicate: return "dup";
        case O::Restarted: return "restart";
        case O::TableFull: return "table-full";
    }
    return "unknown";
}

std::size_t formatReportJson(const protocol::SensorReport& report, const ReportMeta& meta,
                             std::span<char> out) {
    using protocol::hasFlag;
    using protocol::ReportFlags;
    Writer w{out};
    w.text(R"({"node":")").hex8(report.nodeId);
    w.text(R"(","seq":)").number(report.sequence);
    w.text(R"(,"distance_mm":)").number(report.distanceMm);
    w.text(R"(,"distance_valid":)").boolean(hasFlag(report.flags, ReportFlags::DistanceValid));
    w.text(R"(,"motion":)").boolean(hasFlag(report.flags, ReportFlags::Motion));
    w.text(R"(,"cold_boot":)").boolean(hasFlag(report.flags, ReportFlags::ColdBoot));
    w.text(R"(,"battery_mv":)").number(report.batteryMv);
    w.text(R"(,"rssi":)").number(static_cast<int>(meta.rssi));
    w.text(R"(,"link":")").text(outcomeName(meta.outcome));
    w.text(R"(","lost":)").number(meta.lostTotal).text("}");
    return w.finish();
}

std::size_t formatNodeTopic(std::string_view prefix, std::uint32_t gatewayId, std::uint32_t nodeId,
                            std::span<char> out) {
    Writer w{out};
    w.text(prefix).text("/").hex8(gatewayId).text("/node/").hex8(nodeId);
    return w.finish();
}

std::size_t formatStatusTopic(std::string_view prefix, std::uint32_t gatewayId,
                              std::span<char> out) {
    Writer w{out};
    w.text(prefix).text("/").hex8(gatewayId).text("/status");
    return w.finish();
}

}  // namespace sn::app
