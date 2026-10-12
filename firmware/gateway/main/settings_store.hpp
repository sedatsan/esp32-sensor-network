#pragma once

#include <optional>

#include "esp_err.h"
#include "sn/app/gateway_settings.hpp"

namespace gw {

// Settings live in NVS so they survive reflashing the same partition layout
// and are never part of the firmware image.
std::optional<sn::app::GatewaySettings> loadSettings();
esp_err_t saveSettings(const sn::app::GatewaySettings& settings);
esp_err_t eraseSettings();

}  // namespace gw
