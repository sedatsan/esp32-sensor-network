#pragma once

#include <cstdint>

#include "esp_err.h"
#include "sn/app/gateway_settings.hpp"
#include "sn/app/telemetry.hpp"
#include "sn/protocol/report.hpp"

namespace gw {

// Normal mode: joins the configured Wi-Fi network as a station and keeps an
// MQTT session to the broker (TLS via the ESP-IDF certificate bundle). The
// broker sees "<prefix>/<gateway>/status" = "online", or "offline" through the
// last will when the gateway drops off. Requires esp_netif_init() and the
// default event loop; Wi-Fi must be initialised but not started.
esp_err_t startUplink(const sn::app::GatewaySettings& settings, std::uint32_t gatewayId);

// Queues a report for publishing (QoS 1). Never blocks on the network; while
// the broker is unreachable reports wait in a bounded outbox.
void publishReport(const sn::protocol::SensorReport& report, const sn::app::ReportMeta& meta);

}  // namespace gw
