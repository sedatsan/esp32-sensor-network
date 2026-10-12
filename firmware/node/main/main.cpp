// Sensor node (ESP32-C3 Super Mini): wake, sample, report over ESP-NOW,
// deep sleep. All decisions live in sn::app::NodeController; this file only
// wires the ESP-IDF implementations together.

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sn/app/node_controller.hpp"
#include "sn/drivers/hcsr04.hpp"
#include "sn/drivers/pir.hpp"
#include "sn/esp/esp_hal.hpp"
#include "sn/esp/espnow_link.hpp"

namespace {

constexpr const char* kTag = "node";
constexpr auto kPinPir = static_cast<gpio_num_t>(CONFIG_SN_PIN_PIR);
constexpr auto kPinTrig = static_cast<gpio_num_t>(CONFIG_SN_PIN_TRIG);
constexpr auto kPinEcho = static_cast<gpio_num_t>(CONFIG_SN_PIN_ECHO);
constexpr std::size_t kDistanceSamples = 5;
constexpr std::uint32_t kRangerPowerUpMs = 100;

// Survives deep sleep; cleared on power loss, which the gateway sees as a
// ColdBoot report.
RTC_DATA_ATTR sn::app::RetainedState gRetained;

class NodeSensors final : public sn::app::ISensors {
public:
    NodeSensors(sn::drivers::Pir& pir, sn::drivers::Hcsr04& ranger) : pir_(pir), ranger_(ranger) {}

    bool motion() override {
        const bool detected = pir_.motionDetected();
        ESP_LOGI(kTag, "motion %s%s", detected ? "yes" : "no",
                 pir_.isReady() ? "" : " (PIR warming up)");
        return detected;
    }

    std::uint16_t distanceMm() override {
        const auto result = ranger_.measureMedianMm(kDistanceSamples);
        if (!result) {
            ESP_LOGW(kTag, "distance failed: error %d", static_cast<int>(result.error()));
            return 0;
        }
        ESP_LOGI(kTag, "distance %u mm", *result);
        return *result;
    }

    // No battery divider on the breadboard yet.
    std::uint16_t batteryMv() override { return 0; }

private:
    sn::drivers::Pir& pir_;
    sn::drivers::Hcsr04& ranger_;
};

sn::app::WakeCause wakeCause() {
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: return sn::app::WakeCause::Timer;
        case ESP_SLEEP_WAKEUP_GPIO:
        case ESP_SLEEP_WAKEUP_EXT0: return sn::app::WakeCause::Motion;
        default: return sn::app::WakeCause::ColdBoot;
    }
}

[[noreturn, maybe_unused]] void enterDeepSleep(const sn::app::SleepPlan& plan) {
    esp_sleep_enable_timer_wakeup(static_cast<std::uint64_t>(plan.timerWakeS) * 1'000'000ULL);
    if (plan.wakeOnMotion) {
#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
        ESP_ERROR_CHECK(
            esp_deep_sleep_enable_gpio_wakeup(1ULL << kPinPir, ESP_GPIO_WAKEUP_GPIO_HIGH));
#else
        ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(kPinPir, 1));
#endif
    }
    ESP_LOGI(kTag, "sleeping %lus, motion wake %s", static_cast<unsigned long>(plan.timerWakeS),
             plan.wakeOnMotion ? "on" : "off");
    esp_deep_sleep_start();
}

std::uint32_t nodeIdFromMac() {
    std::uint8_t mac[6]{};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    return (static_cast<std::uint32_t>(mac[2]) << 24) | (static_cast<std::uint32_t>(mac[3]) << 16) |
           (static_cast<std::uint32_t>(mac[4]) << 8) | mac[5];
}

sn::app::SleepPlan runAndLog(sn::app::NodeController& controller, sn::app::WakeCause cause) {
    ESP_LOGI(kTag, "wake cause %d, seq %lu", static_cast<int>(cause),
             static_cast<unsigned long>(gRetained.sequence));
    const std::uint32_t failedBefore = gRetained.failedReports;
    const auto plan = controller.runCycle(cause, gRetained);
    if (gRetained.failedReports != failedBefore) {
        ESP_LOGW(kTag, "report not acknowledged by gateway (%lu failed so far)",
                 static_cast<unsigned long>(gRetained.failedReports));
    } else {
        ESP_LOGI(kTag, "report delivered");
    }
    return plan;
}

}  // namespace

extern "C" void app_main() {
    const auto cause = wakeCause();

    sn::esp::EspClock clock;
    sn::esp::EspOutputPin trig{kPinTrig};
    sn::esp::EspInputPin echo{kPinEcho, GPIO_FLOATING};
    sn::esp::EspInputPin pirOut{kPinPir, GPIO_PULLDOWN_ONLY};

    sn::drivers::Hcsr04 ranger{trig, echo, clock};
    sn::drivers::Pir pir{pirOut, clock};
    if (cause != sn::app::WakeCause::ColdBoot) {
        pir.skipWarmup();  // the PIR stayed powered while we slept
    } else {
        // The HC-SR04 ignores the first trigger right after power-up.
        vTaskDelay(pdMS_TO_TICKS(kRangerPowerUpMs));
    }
    NodeSensors sensors{pir, ranger};

    sn::esp::MacAddress gateway{};
    if (!sn::esp::parseMac(CONFIG_SN_GATEWAY_MAC, gateway)) {
        ESP_LOGE(kTag, "invalid CONFIG_SN_GATEWAY_MAC");
        gateway.fill(0xFF);
    }
    ESP_ERROR_CHECK(sn::esp::initRadio(CONFIG_SN_ESPNOW_CHANNEL));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(CONFIG_SN_MAX_TX_POWER_QDBM));
    sn::esp::EspNowTransport transport;
    ESP_ERROR_CHECK(transport.begin(gateway, CONFIG_SN_ESPNOW_CHANNEL));

    const sn::app::NodeConfig config{
        .nodeId = nodeIdFromMac(),
        .reportIntervalS = CONFIG_SN_REPORT_INTERVAL_S,
    };
    sn::app::NodeController controller{sensors, transport, clock, config};

#if CONFIG_SN_DEBUG_STAY_AWAKE
    // Bench mode: never sleep, so the USB console stays attached.
    for (auto c = cause;; c = sn::app::WakeCause::Timer) {
        (void)runAndLog(controller, c);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SN_DEBUG_PERIOD_MS));
    }
#else
    enterDeepSleep(runAndLog(controller, cause));
#endif
}
