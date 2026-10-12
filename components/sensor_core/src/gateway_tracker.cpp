#include "sn/app/gateway_tracker.hpp"

#include <algorithm>
#include <iterator>
#include <span>

namespace sn::app {

std::size_t GatewayTracker::indexOf(std::uint32_t nodeId) const {
    const auto used = std::span{nodes_}.first(count_);
    const auto it = std::find_if(used.begin(), used.end(),
                                 [nodeId](const NodeStats& n) { return n.nodeId == nodeId; });
    return static_cast<std::size_t>(std::distance(used.begin(), it));
}

const GatewayTracker::NodeStats* GatewayTracker::find(std::uint32_t nodeId) const {
    const std::size_t i = indexOf(nodeId);
    return i < count_ ? &nodes_[i] : nullptr;
}

GatewayTracker::Result GatewayTracker::onReport(const protocol::SensorReport& report) {
    const std::size_t index = indexOf(report.nodeId);
    NodeStats* node = index < count_ ? &nodes_[index] : nullptr;
    if (node == nullptr) {
        if (count_ == kMaxNodes) {
            return {Outcome::TableFull, 0};
        }
        node = &nodes_[count_++];
        *node = NodeStats{.nodeId = report.nodeId, .lastSequence = report.sequence, .received = 1};
        return {Outcome::NewNode, 0};
    }

    // A cold boot clears the node's RTC memory, so its sequence restarts.
    if (protocol::hasFlag(report.flags, protocol::ReportFlags::ColdBoot)) {
        node->lastSequence = report.sequence;
        ++node->received;
        ++node->restarts;
        return {Outcome::Restarted, 0};
    }

    // Unsigned subtraction handles sequence wrap-around.
    const std::uint32_t delta = report.sequence - node->lastSequence;
    if (delta == 0 || delta > UINT32_MAX / 2) {
        ++node->duplicates;
        return {Outcome::Duplicate, 0};
    }

    node->lastSequence = report.sequence;
    ++node->received;
    const std::uint32_t lost = delta - 1;
    node->lost += lost;
    return {lost == 0 ? Outcome::InOrder : Outcome::Gap, lost};
}

}  // namespace sn::app
