#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "sn/app/gateway_tracker.hpp"
#include "sn/protocol/report.hpp"

namespace sn::app {

// What the gateway knows about a report besides its payload.
struct ReportMeta {
    std::int8_t rssi = 0;
    GatewayTracker::Outcome outcome = GatewayTracker::Outcome::InOrder;
    std::uint32_t lostTotal = 0;
};

// Short lowercase name used in logs and JSON ("ok", "gap", ...).
[[nodiscard]] std::string_view outcomeName(GatewayTracker::Outcome outcome);

// The formatters below write a NUL-terminated string into `out` and return
// its length, or 0 if `out` is too small (nothing useful is written then).
// No heap, no printf, so they behave the same on the host and on target.

// {"node":"d51eb380","seq":1,"distance_mm":172,"distance_valid":true,
//  "motion":true,"cold_boot":false,"battery_mv":0,"rssi":-69,"link":"ok","lost":0}
std::size_t formatReportJson(const protocol::SensorReport& report, const ReportMeta& meta,
                             std::span<char> out);

// <prefix>/<gateway>/node/<node>, ids as 8 lowercase hex digits.
std::size_t formatNodeTopic(std::string_view prefix, std::uint32_t gatewayId, std::uint32_t nodeId,
                            std::span<char> out);

// <prefix>/<gateway>/status, retained "online"/"offline" (MQTT last will).
std::size_t formatStatusTopic(std::string_view prefix, std::uint32_t gatewayId,
                              std::span<char> out);

}  // namespace sn::app
