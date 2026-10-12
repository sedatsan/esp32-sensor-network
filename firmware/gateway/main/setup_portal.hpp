#pragma once

#include <cstdint>

#include "esp_err.h"

namespace gw {

// Setup mode: a WPA2 access point "SensorNet-<id>" with a one-page form at
// http://192.168.4.1. Saving the form stores the settings in NVS and reboots
// into normal operation. ESP-NOW keeps working on `channel` meanwhile.
// Requires esp_netif_init() and the default event loop.
esp_err_t startSetupPortal(std::uint8_t channel, std::uint32_t gatewayId);

}  // namespace gw
