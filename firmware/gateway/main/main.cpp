// Gateway (ESP32 DevKit V1): receives ESP-NOW reports, validates them and
// tracks per-node link quality. The radio callback only copies the frame into
// a queue; decoding happens in a dedicated FreeRTOS task.

#include <cstring>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sn/app/gateway_tracker.hpp"
#include "sn/esp/esp_hal.hpp"
#include "sn/esp/espnow_link.hpp"
#include "sn/protocol/report.hpp"

namespace {

constexpr const char* kTag = "gateway";
constexpr auto kPinLed = static_cast<gpio_num_t>(CONFIG_SN_PIN_STATUS_LED);
constexpr UBaseType_t kQueueDepth = 16;
constexpr std::uint32_t kReportTaskStack = 4096;
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

const char* outcomeName(sn::app::GatewayTracker::Outcome o) {
    using O = sn::app::GatewayTracker::Outcome;
    switch (o) {
        case O::NewNode: return "new";
        case O::InOrder: return "ok";
        case O::Gap: return "gap";
        case O::Duplicate: return "dup";
        case O::Restarted: return "restart";
        case O::TableFull: return "table-full";
    }
    return "?";
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
            ESP_LOGI(kTag, "node %08lx seq %lu dist %u mm motion %d rssi %d [%s, lost %lu]",
                     static_cast<unsigned long>(report->nodeId),
                     static_cast<unsigned long>(report->sequence), report->distanceMm,
                     sn::protocol::hasFlag(report->flags, sn::protocol::ReportFlags::Motion),
                     frame.rssi, outcomeName(result.outcome),
                     static_cast<unsigned long>(result.lostNow));
            // TODO(milestone 5): publish to MQTT over TLS.
        }
        led.write(false);
    }
}

}  // namespace

extern "C" void app_main() {
    gFrames = xQueueCreate(kQueueDepth, sizeof(Frame));
    configASSERT(gFrames != nullptr);

    ESP_ERROR_CHECK(sn::esp::initRadio(CONFIG_SN_ESPNOW_CHANNEL));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onReceive));

    std::uint8_t mac[6]{};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(kTag, "gateway MAC " MACSTR " (set this as SN_GATEWAY_MAC on nodes), channel %d",
             MAC2STR(mac), CONFIG_SN_ESPNOW_CHANNEL);

    xTaskCreate(reportTask, "reports", kReportTaskStack, nullptr, 5, nullptr);
}
