#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "sn/expected.hpp"
#include "sn/fixed_string.hpp"

namespace sn::app {

// Everything the gateway needs to reach the broker. Entered by the user in the
// setup portal and stored in NVS; never compiled into the firmware.
struct GatewaySettings {
    FixedString<32> wifiSsid;      // 802.11 limit
    FixedString<63> wifiPassword;  // WPA2 passphrase: empty (open) or 8..63
    FixedString<127> brokerUri;    // mqtts://host:port or mqtt://host:port
    FixedString<64> brokerUser;
    FixedString<64> brokerPassword;
};

enum class SettingsError : std::uint8_t {
    BadEncoding,  // malformed %XX escape
    FieldTooLong,
    MissingSsid,
    BadWifiPassword,  // 1..7 characters
    BadBrokerUri,     // wrong scheme or no host
};

[[nodiscard]] std::string_view settingsErrorText(SettingsError error);

// Decodes one application/x-www-form-urlencoded value ('+' is a space,
// %XX a byte). Returns the decoded length; fails if `out` is too small.
[[nodiscard]] Expected<std::size_t, SettingsError> urlDecode(std::string_view in,
                                                             std::span<char> out);

// Parses the setup form body: ssid, wifi_pass, broker_uri, broker_user,
// broker_pass. Unknown fields are ignored, missing ones are empty.
[[nodiscard]] Expected<GatewaySettings, SettingsError> parseSetupForm(std::string_view body);

// Checks a settings struct, e.g. one loaded back from NVS.
[[nodiscard]] Expected<bool, SettingsError> validate(const GatewaySettings& settings);

// True for mqtts:// URIs; plain mqtt:// is only meant for a local test broker.
[[nodiscard]] bool usesTls(const GatewaySettings& settings);

}  // namespace sn::app
