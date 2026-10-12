#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sn/protocol/report.hpp"

namespace sn::app {

// Keeps per-node link statistics on the gateway: detects lost, duplicated
// and restarted reports from the sequence numbers. Fixed capacity, no heap.
class GatewayTracker {
public:
    enum class Outcome : std::uint8_t { NewNode, InOrder, Gap, Duplicate, Restarted, TableFull };

    struct NodeStats {
        std::uint32_t nodeId = 0;
        std::uint32_t lastSequence = 0;
        std::uint32_t received = 0;
        std::uint32_t lost = 0;
        std::uint32_t duplicates = 0;
        std::uint32_t restarts = 0;
    };

    struct Result {
        Outcome outcome;
        std::uint32_t lostNow;  // reports missed just before this one
    };

    static constexpr std::size_t kMaxNodes = 16;

    Result onReport(const protocol::SensorReport& report);
    [[nodiscard]] const NodeStats* find(std::uint32_t nodeId) const;
    [[nodiscard]] std::size_t nodeCount() const { return count_; }

private:
    // Index of the node, or count_ when unknown.
    [[nodiscard]] std::size_t indexOf(std::uint32_t nodeId) const;

    std::array<NodeStats, kMaxNodes> nodes_{};
    std::size_t count_ = 0;
};

}  // namespace sn::app
