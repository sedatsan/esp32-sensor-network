// Gateway (ESP32 DevKit V1): receives ESP-NOW reports, validates them,
// tracks per-node link quality and publishes them over MQTT. The radio
// callback only copies the frame into a queue; decoding and publishing happen
// in a dedicated FreeRTOS task.
//
// First boot (or BOOT held for 3 s): setup mode with an access point and a
// web form for Wi-Fi and broker settings. Afterwards: station + MQTT.

#include <cstring>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "settings_store.hpp"
#include "setup_portal.hpp"
#include "sn/app/gateway_tracker.hpp"
#include "sn/app/long_press.hpp"
#include "sn/app/telemetry.hpp"
#include "sn/esp/esp_hal.hpp"
#include "sn/esp/espnow_link.hpp"
#include "sn/protocol/report.hpp"
#include "uplink.hpp"

namespace {

constexpr const char* kTag = "gateway";
constexpr auto kPinLed = static_cast<gpio_num_t>(CONFIG_SN_PIN_STATUS_LED);
constexpr auto kPinBoot = GPIO_NUM_0;  // BOOT button on the DevKit, active low
constexpr std::uint32_t kFactoryResetHoldMs = 3000;
constexpr std::uint32_t kButtonPollMs = 50;
constexpr UBaseType_t kQueueDepth = 16;
constexpr std::uint32_t kReportTaskStack = 6144;
constexpr std::uint32_t kButtonTaskStack = 3072;
constexpr std::size_t kMaxFrame = 32;

struct Frame {
    std::uint8_t src[6];
    std::int8_t rssi;
    std::uint8_t len;
    std::uint8_t data[kMaxFrame];
};

QueueHandle_t gFrames = nullptr;
std::uint32_t gDroppedFrames = 0;  // only written from the Wi-Fi task

void onReceive(const esp_now_recv_info_t* info, const std::uint8_t* data, int len) {
    if (len <= 0 || static_cast<std::size_t>(len) > kMaxFrame) {
        return;
    }
    Frame frame{};
    std::memcpy(frame.src, info->src_addr, sizeof(frame.src));
    frame.rssi = static_cast<std::int8_t>(info->rx_ctrl->rssi);
    frame.len = static_cast<std::uint8_t>(len);
    std::memcpy(frame.data, data, static_cast<std::size_t>(len));
    if (xQueueSend(gFrames, &frame, 0) != pdTRUE) {
        ++gDroppedFrames;
    }
}

void reportTask(void* /*arg*/) {
    sn::esp::EspOutputPin led{kPinLed};
    sn::app::GatewayTracker tracker;
    Frame frame{};

    for (;;) {
        if (xQueueReceive(gFrames, &frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        led.write(true);
        const auto report = sn::protocol::decode(std::span{frame.data, frame.len});
        if (!report) {
            ESP_LOGW(kTag, "bad frame from " MACSTR ": error %d", MAC2STR(frame.src),
                     static_cast<int>(report.error()));
        } else {
            const auto result = tracker.onReport(*report);
            const auto link = sn::app::outcomeName(result.outcome);
            ESP_LOGI(kTag, "node %08lx seq %lu dist %u mm motion %d rssi %d [%.*s, lost %lu]",
                     static_cast<unsigned long>(report->nodeId),
                     static_cast<unsigned long>(report->sequence), report->distanceMm,
                     sn::protocol::hasFlag(report->flags, sn::protocol::ReportFlags::Motion),
                     frame.rssi, static_cast<int>(link.size()), link.data(),
                     static_cast<unsigned long>(result.lostNow));
            const auto* stats = tracker.find(report->nodeId);
            gw::publishReport(*report, {.rssi = frame.rssi,
                                        .outcome = result.outcome,
                                        .lostTotal = stats != nullptr ? stats->lost : 0});
        }
        led.write(false);
    }
}

// Holding BOOT for 3 s wipes the stored settings and reboots into setup mode.
void buttonTask(void* /*arg*/) {
    const sn::esp::EspInputPin boot{kPinBoot, GPIO_PULLUP_ONLY};
    sn::app::LongPressDetector longPress{kFactoryResetHoldMs};
    for (;;) {
        const auto nowMs = static_cast<std::uint32_t>(pdTICKS_TO_MS(xTaskGetTickCount()));
        if (longPress.update(!boot.read(), nowMs)) {
            ESP_LOGW(kTag, "BOOT held: erasing settings and restarting into setup mode");
            ESP_ERROR_CHECK_WITHOUT_ABORT(gw::eraseSettings());
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(kButtonPollMs));
    }
}

std::uint32_t gatewayIdFromMac(const std::uint8_t (&mac)[6]) {
    return (static_cast<std::uint32_t>(mac[2]) << 24) | (static_cast<std::uint32_t>(mac[3]) << 16) |
           (static_cast<std::uint32_t>(mac[4]) << 8) | mac[5];
}

}  // namespace

extern "C" void app_main() {
    gFrames = xQueueCreate(kQueueDepth, sizeof(Frame));
    configASSERT(gFrames != nullptr);

    ESP_ERROR_CHECK(sn::esp::initNvs());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    std::uint8_t mac[6]{};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    const std::uint32_t gatewayId = gatewayIdFromMac(mac);
    ESP_LOGI(kTag, "gateway MAC " MACSTR " (set this as SN_GATEWAY_MAC on nodes), id %08lx",
             MAC2STR(mac), static_cast<unsigned long>(gatewayId));

    const auto settings = gw::loadSettings();
    if (settings) {
        esp_netif_create_default_wifi_sta();
    } else {
        esp_netif_create_default_wifi_ap();
    }
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    if (settings) {
        ESP_ERROR_CHECK(gw::startUplink(*settings, gatewayId));
    } else {
        ESP_ERROR_CHECK(gw::startSetupPortal(CONFIG_SN_ESPNOW_CHANNEL, gatewayId));
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onReceive));

    xTaskCreate(reportTask, "reports", kReportTaskStack, nullptr, 5, nullptr);
    xTaskCreate(buttonTask, "button", kButtonTaskStack, nullptr, 2, nullptr);
}
