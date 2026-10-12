#include "settings_store.hpp"

#include <type_traits>

#include "esp_log.h"
#include "nvs.h"

namespace gw {
namespace {

constexpr const char* kTag = "settings";
constexpr const char* kNamespace = "gateway";
constexpr const char* kKeyBlob = "settings";
constexpr const char* kKeyVersion = "version";
// Bump when GatewaySettings changes layout; older blobs are then ignored.
constexpr std::uint8_t kLayoutVersion = 1;

static_assert(std::is_trivially_copyable_v<sn::app::GatewaySettings>);

class NvsHandle {
public:
    esp_err_t open(nvs_open_mode_t mode) { return nvs_open(kNamespace, mode, &handle_); }
    ~NvsHandle() {
        if (handle_ != 0) {
            nvs_close(handle_);
        }
    }
    nvs_handle_t get() const { return handle_; }

private:
    nvs_handle_t handle_ = 0;
};

}  // namespace

std::optional<sn::app::GatewaySettings> loadSettings() {
    NvsHandle nvs;
    if (nvs.open(NVS_READONLY) != ESP_OK) {
        return std::nullopt;  // namespace does not exist yet: first boot
    }
    std::uint8_t version = 0;
    sn::app::GatewaySettings settings;
    std::size_t size = sizeof(settings);
    if (nvs_get_u8(nvs.get(), kKeyVersion, &version) != ESP_OK || version != kLayoutVersion ||
        nvs_get_blob(nvs.get(), kKeyBlob, &settings, &size) != ESP_OK || size != sizeof(settings)) {
        return std::nullopt;
    }
    if (const auto valid = sn::app::validate(settings); !valid) {
        ESP_LOGW(kTag, "stored settings rejected: %.*s",
                 static_cast<int>(sn::app::settingsErrorText(valid.error()).size()),
                 sn::app::settingsErrorText(valid.error()).data());
        return std::nullopt;
    }
    return settings;
}

esp_err_t saveSettings(const sn::app::GatewaySettings& settings) {
    NvsHandle nvs;
    esp_err_t err = nvs.open(NVS_READWRITE);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs.get(), kKeyBlob, &settings, sizeof(settings));
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs.get(), kKeyVersion, kLayoutVersion);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs.get());
    }
    return err;
}

esp_err_t eraseSettings() {
    NvsHandle nvs;
    esp_err_t err = nvs.open(NVS_READWRITE);
    if (err == ESP_OK) {
        err = nvs_erase_all(nvs.get());
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs.get());
    }
    return err;
}

}  // namespace gw
