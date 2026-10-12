#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "esp_err.h"
#include "sn/app/node_controller.hpp"

namespace sn::esp {

using MacAddress = std::array<std::uint8_t, 6>;

// Brings up Wi-Fi in station mode (radio only, no AP connection) on a fixed
// channel, which is all ESP-NOW needs.
esp_err_t initRadio(std::uint8_t channel);

// Parses "AA:BB:CC:DD:EE:FF". Returns false on malformed input.
bool parseMac(const char* text, MacAddress& out);

// Node side: unicast to the gateway and wait for the MAC-layer ACK.
class EspNowTransport final : public app::ITransport {
public:
    esp_err_t begin(const MacAddress& peer, std::uint8_t channel);
    bool send(std::span<const std::uint8_t> payload) override;
};

}  // namespace sn::esp
