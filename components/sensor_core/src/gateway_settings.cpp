#include "sn/app/gateway_settings.hpp"

#include <array>

namespace sn::app {
namespace {

constexpr std::string_view kTlsScheme = "mqtts://";
constexpr std::string_view kPlainScheme = "mqtt://";
constexpr std::size_t kMinWpaPassphrase = 8;

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool brokerUriValid(std::string_view uri) {
    std::string_view rest;
    if (uri.starts_with(kTlsScheme)) {
        rest = uri.substr(kTlsScheme.size());
    } else if (uri.starts_with(kPlainScheme)) {
        rest = uri.substr(kPlainScheme.size());
    } else {
        return false;
    }
    const auto hostEnd = rest.find_first_of(":/");
    const auto host = rest.substr(0, hostEnd);
    return !host.empty() && host.find(' ') == std::string_view::npos;
}

template <std::size_t N>
Expected<bool, SettingsError> store(std::string_view encoded, FixedString<N>& field) {
    std::array<char, N> decoded{};
    const auto length = urlDecode(encoded, decoded);
    if (!length) {
        return unexpected(length.error());
    }
    field.assign({decoded.data(), *length});
    return true;
}

}  // namespace

std::string_view settingsErrorText(SettingsError error) {
    switch (error) {
        case SettingsError::BadEncoding: return "The form data was malformed.";
        case SettingsError::FieldTooLong: return "One of the fields is too long.";
        case SettingsError::MissingSsid: return "Enter the Wi-Fi network name.";
        case SettingsError::BadWifiPassword:
            return "Wi-Fi passwords are 8 to 63 characters (leave empty for an open network).";
        case SettingsError::BadBrokerUri:
            return "The broker address must look like mqtts://host:8883.";
    }
    return "Invalid settings.";
}

Expected<std::size_t, SettingsError> urlDecode(std::string_view in, std::span<char> out) {
    std::size_t used = 0;
    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            if (in.size() - i < 3) {
                return unexpected(SettingsError::BadEncoding);
            }
            const int hi = hexValue(in[i + 1]);
            const int lo = hexValue(in[i + 2]);
            if (hi < 0 || lo < 0) {
                return unexpected(SettingsError::BadEncoding);
            }
            c = static_cast<char>((hi << 4) | lo);
            i += 2;
        }
        if (used == out.size()) {
            return unexpected(SettingsError::FieldTooLong);
        }
        out[used++] = c;
    }
    return used;
}

Expected<GatewaySettings, SettingsError> parseSetupForm(std::string_view body) {
    GatewaySettings settings;
    while (!body.empty()) {
        const auto amp = body.find('&');
        const auto pair = body.substr(0, amp);
        body = amp == std::string_view::npos ? std::string_view{} : body.substr(amp + 1);

        const auto eq = pair.find('=');
        const auto key = pair.substr(0, eq);
        const auto value = eq == std::string_view::npos ? std::string_view{} : pair.substr(eq + 1);

        Expected<bool, SettingsError> stored = true;
        if (key == "ssid") {
            stored = store(value, settings.wifiSsid);
        } else if (key == "wifi_pass") {
            stored = store(value, settings.wifiPassword);
        } else if (key == "broker_uri") {
            stored = store(value, settings.brokerUri);
        } else if (key == "broker_user") {
            stored = store(value, settings.brokerUser);
        } else if (key == "broker_pass") {
            stored = store(value, settings.brokerPassword);
        }
        if (!stored) {
            return unexpected(stored.error());
        }
    }
    if (const auto valid = validate(settings); !valid) {
        return unexpected(valid.error());
    }
    return settings;
}

Expected<bool, SettingsError> validate(const GatewaySettings& settings) {
    if (settings.wifiSsid.empty()) {
        return unexpected(SettingsError::MissingSsid);
    }
    if (!settings.wifiPassword.empty() && settings.wifiPassword.size() < kMinWpaPassphrase) {
        return unexpected(SettingsError::BadWifiPassword);
    }
    if (!brokerUriValid(settings.brokerUri.view())) {
        return unexpected(SettingsError::BadBrokerUri);
    }
    return true;
}

bool usesTls(const GatewaySettings& settings) {
    return settings.brokerUri.view().starts_with(kTlsScheme);
}

}  // namespace sn::app
