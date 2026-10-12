#include "uplink.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "sdkconfig.h"

namespace gw {
namespace {

constexpr const char* kTag = "uplink";
constexpr const char* kOnline = "online";
constexpr const char* kOffline = "offline";
constexpr std::uint64_t kOutboxLimitBytes = 16 * 1024;
constexpr int kQos = 1;

// Owned by this module for the lifetime of the firmware. The settings are
// copied because esp-mqtt keeps pointers into its config strings.
sn::app::GatewaySettings gSettings;
std::uint32_t gGatewayId = 0;
std::array<char, 64> gStatusTopic{};
std::array<char, 24> gClientId{};
esp_mqtt_client_handle_t gClient = nullptr;
bool gMqttStarted = false;
volatile bool gMqttConnected = false;

void onWifiEvent(void* /*arg*/, esp_event_base_t base, std::int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const auto* info = static_cast<const wifi_event_sta_disconnected_t*>(data);
        ESP_LOGW(kTag, "Wi-Fi disconnected (reason %d), retrying", info->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* info = static_cast<const ip_event_got_ip_t*>(data);
        std::uint8_t channel = 0;
        wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
        esp_wifi_get_channel(&channel, &second);
        ESP_LOGI(kTag, "Wi-Fi up, IP " IPSTR ", channel %u (nodes follow automatically)",
                 IP2STR(&info->ip_info.ip), channel);
        if (!gMqttStarted) {
            gMqttStarted = esp_mqtt_client_start(gClient) == ESP_OK;
        }
    }
}

void onMqttEvent(void* /*arg*/, esp_event_base_t /*base*/, std::int32_t id, void* data) {
    const auto* event = static_cast<esp_mqtt_event_handle_t>(data);
    switch (static_cast<esp_mqtt_event_id_t>(id)) {
        case MQTT_EVENT_CONNECTED:
            gMqttConnected = true;
            ESP_LOGI(kTag, "MQTT connected to %s", gSettings.brokerUri.c_str());
            esp_mqtt_client_publish(gClient, gStatusTopic.data(), kOnline, 0, kQos, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            gMqttConnected = false;
            ESP_LOGW(kTag, "MQTT disconnected, will reconnect");
            break;
        case MQTT_EVENT_ERROR:
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(kTag,
                         "broker refused the connection (code %d) for user \"%s\" with a %u-char "
                         "password: check both (case-sensitive); hold BOOT 3 s to re-enter them",
                         static_cast<int>(event->error_handle->connect_return_code),
                         gSettings.brokerUser.c_str(),
                         static_cast<unsigned>(gSettings.brokerPassword.size()));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(kTag, "transport error: tls 0x%x, stack 0x%x, socket errno %d",
                         event->error_handle->esp_tls_last_esp_err,
                         event->error_handle->esp_tls_stack_err,
                         event->error_handle->esp_transport_sock_errno);
            }
            break;
        default: break;
    }
}

}  // namespace

esp_err_t startUplink(const sn::app::GatewaySettings& settings, std::uint32_t gatewayId) {
    gSettings = settings;
    gGatewayId = gatewayId;
    sn::app::formatStatusTopic(CONFIG_SN_MQTT_TOPIC_PREFIX, gatewayId, gStatusTopic);
    std::snprintf(gClientId.data(), gClientId.size(), "sn-gw-%08lx",
                  static_cast<unsigned long>(gatewayId));

    esp_mqtt_client_config_t mqtt{};
    mqtt.broker.address.uri = gSettings.brokerUri.c_str();
    if (sn::app::usesTls(gSettings)) {
        mqtt.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    } else {
        ESP_LOGW(kTag, "broker URI is not mqtts://: traffic is NOT encrypted");
    }
    if (!gSettings.brokerUser.empty()) {
        mqtt.credentials.username = gSettings.brokerUser.c_str();
        mqtt.credentials.authentication.password = gSettings.brokerPassword.c_str();
    }
    mqtt.credentials.client_id = gClientId.data();
    mqtt.session.last_will.topic = gStatusTopic.data();
    mqtt.session.last_will.msg = kOffline;
    mqtt.session.last_will.qos = kQos;
    mqtt.session.last_will.retain = 1;
    mqtt.session.keepalive = 30;
    mqtt.outbox.limit = kOutboxLimitBytes;
    gClient = esp_mqtt_client_init(&mqtt);
    if (gClient == nullptr) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_mqtt_client_register_event(gClient, MQTT_EVENT_ANY, onMqttEvent, nullptr);
    if (err != ESP_OK) {
        return err;
    }

    if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, nullptr)) !=
            ESP_OK ||
        (err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onWifiEvent, nullptr)) !=
            ESP_OK) {
        return err;
    }

    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, gSettings.wifiSsid.c_str(), gSettings.wifiSsid.size());
    std::memcpy(sta.sta.password, gSettings.wifiPassword.c_str(), gSettings.wifiPassword.size());
    sta.sta.threshold.authmode =
        gSettings.wifiPassword.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    sta.sta.pmf_cfg.capable = true;

    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK ||
        (err = esp_wifi_set_config(WIFI_IF_STA, &sta)) != ESP_OK ||
        (err = esp_wifi_start()) != ESP_OK) {
        return err;
    }
    // Modem sleep would make the radio miss ESP-NOW frames between beacons.
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(kTag, "joining Wi-Fi \"%s\"", gSettings.wifiSsid.c_str());
    return ESP_OK;
}

void publishReport(const sn::protocol::SensorReport& report, const sn::app::ReportMeta& meta) {
    if (gClient == nullptr) {
        return;  // setup mode: nothing to publish to
    }
    std::array<char, 64> topic{};
    std::array<char, 256> json{};
    const auto topicLen =
        sn::app::formatNodeTopic(CONFIG_SN_MQTT_TOPIC_PREFIX, gGatewayId, report.nodeId, topic);
    const auto jsonLen = sn::app::formatReportJson(report, meta, json);
    if (topicLen == 0 || jsonLen == 0) {
        ESP_LOGE(kTag, "report does not fit the publish buffers");
        return;
    }
    const int id = esp_mqtt_client_enqueue(gClient, topic.data(), json.data(),
                                           static_cast<int>(jsonLen), kQos, 0, true);
    if (id < 0) {
        ESP_LOGW(kTag, "outbox full, report dropped (broker %s)",
                 gMqttConnected ? "connected" : "unreachable");
    }
}

}  // namespace gw
