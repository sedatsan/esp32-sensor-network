#include "sn/esp/espnow_link.hpp"

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

namespace sn::esp {
namespace {

constexpr TickType_t kAckTimeout = pdMS_TO_TICKS(100);

SemaphoreHandle_t gSendDone = nullptr;
volatile bool gLastSendOk = false;
MacAddress gPeer{};

// Runs in the Wi-Fi task: only record the result and wake the sender.
void onSent(const std::uint8_t* /*mac*/, esp_now_send_status_t status) {
    gLastSendOk = status == ESP_NOW_SEND_SUCCESS;
    xSemaphoreGive(gSendDone);
}

}  // namespace

esp_err_t initRadio(std::uint8_t channel) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }
    if ((err = esp_event_loop_create_default()) != ESP_OK) {
        return err;
    }
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&cfg)) != ESP_OK) {
        return err;
    }
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) {
        return err;
    }
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) {
        return err;
    }
    if ((err = esp_wifi_start()) != ESP_OK) {
        return err;
    }
    if ((err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE)) != ESP_OK) {
        return err;
    }
    return esp_now_init();
}

bool parseMac(const char* text, MacAddress& out) {
    unsigned int b[6];
    if (std::sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) !=
        6) {
        return false;
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(b[i]);
    }
    return true;
}

esp_err_t EspNowTransport::begin(const MacAddress& peer) {
    gPeer = peer;
    gSendDone = xSemaphoreCreateBinary();
    if (gSendDone == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_now_register_send_cb(onSent);
    if (err != ESP_OK) {
        return err;
    }
    esp_now_peer_info_t info{};
    std::memcpy(info.peer_addr, peer.data(), peer.size());
    info.channel = 0;  // 0 = use the radio's current channel
    info.ifidx = WIFI_IF_STA;
    info.encrypt = false;  // TODO(milestone 6): enable LMK encryption
    return esp_now_add_peer(&info);
}

bool EspNowTransport::send(std::span<const std::uint8_t> payload) {
    xSemaphoreTake(gSendDone, 0);  // drop any stale completion
    if (esp_now_send(gPeer.data(), payload.data(), payload.size()) != ESP_OK) {
        return false;
    }
    return xSemaphoreTake(gSendDone, kAckTimeout) == pdTRUE && gLastSendOk;
}

void EspNowTransport::setChannel(std::uint8_t channel) {
    std::uint8_t current = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&current, &second) == ESP_OK && current == channel) {
        return;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
}

}  // namespace sn::esp
