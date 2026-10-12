#include "setup_portal.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "settings_store.hpp"
#include "sn/app/gateway_settings.hpp"

namespace gw {
namespace {

constexpr const char* kTag = "setup";
constexpr std::size_t kMaxBody = 1024;
constexpr std::uint64_t kRebootDelayUs = 1'500'000;

constexpr const char* kPageHead = R"(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>SensorNet setup</title>
<style>body{font-family:system-ui,sans-serif;max-width:28rem;margin:2rem auto;padding:0 1rem;
color:#1d2433}label{display:block;margin-top:1rem;font-weight:600}input{width:100%;
padding:.5rem;margin-top:.25rem;box-sizing:border-box;font:inherit}button{margin-top:1.5rem;
padding:.6rem 1.2rem;font:inherit}.err{background:#fde8e8;padding:.75rem;border-radius:6px}
small{color:#5b6475}</style></head><body><h1>SensorNet gateway</h1>)";

constexpr const char* kForm = R"(<form method="post" action="/save">
<label>Wi-Fi network<input name="ssid" maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false" required></label>
<label>Wi-Fi password<input name="wifi_pass" type="password" maxlength="63" autocapitalize="none" autocorrect="off" spellcheck="false"></label>
<label>Broker address<input name="broker_uri" maxlength="127" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="mqtts://xxxx.hivemq.cloud:8883" required></label>
<label>Broker username<input name="broker_user" maxlength="64" autocapitalize="none" autocorrect="off" spellcheck="false"></label>
<label>Broker password<input name="broker_pass" type="password" maxlength="64" autocapitalize="none" autocorrect="off" spellcheck="false"></label>
<button type="submit">Save and restart</button></form>
<p><small>Settings are stored on this gateway only. Hold BOOT for 3 seconds to come back here.</small></p>
</body></html>)";

esp_err_t sendPage(httpd_req_t* req, const char* message) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, kPageHead);
    if (message != nullptr) {
        httpd_resp_sendstr_chunk(req, "<p class=\"err\">");
        httpd_resp_sendstr_chunk(req, message);
        httpd_resp_sendstr_chunk(req, "</p>");
    }
    httpd_resp_sendstr_chunk(req, kForm);
    return httpd_resp_sendstr_chunk(req, nullptr);
}

esp_err_t handleIndex(httpd_req_t* req) {
    return sendPage(req, nullptr);
}

void rebootSoon() {
    static esp_timer_handle_t timer = nullptr;
    const esp_timer_create_args_t args{
        .callback = [](void*) { esp_restart(); },
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reboot",
        .skip_unhandled_events = true,
    };
    if (timer == nullptr && esp_timer_create(&args, &timer) == ESP_OK) {
        esp_timer_start_once(timer, kRebootDelayUs);
    }
}

esp_err_t handleSave(httpd_req_t* req) {
    if (req->content_len == 0 || req->content_len > kMaxBody) {
        httpd_resp_set_status(req, "413 Content Too Large");
        return sendPage(req, "The form was too large.");
    }
    std::array<char, kMaxBody> body{};
    std::size_t received = 0;
    while (received < req->content_len) {
        const int n = httpd_req_recv(req, body.data() + received, req->content_len - received);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            return ESP_FAIL;
        }
        received += static_cast<std::size_t>(n);
    }

    const auto settings = sn::app::parseSetupForm({body.data(), received});
    if (!settings) {
        httpd_resp_set_status(req, "400 Bad Request");
        return sendPage(req, sn::app::settingsErrorText(settings.error()).data());
    }
    if (const esp_err_t err = saveSettings(*settings); err != ESP_OK) {
        ESP_LOGE(kTag, "saving settings failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        return sendPage(req, "Could not store the settings. Please try again.");
    }
    ESP_LOGI(kTag,
             "settings saved for network \"%s\", broker %s, user \"%s\" (password %u chars); "
             "restarting",
             settings->wifiSsid.c_str(), settings->brokerUri.c_str(), settings->brokerUser.c_str(),
             static_cast<unsigned>(settings->brokerPassword.size()));
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, kPageHead);
    httpd_resp_sendstr_chunk(req,
                             "<p>Saved. The gateway is restarting and will join your network. "
                             "You can close this page.</p></body></html>");
    httpd_resp_sendstr_chunk(req, nullptr);
    rebootSoon();
    return ESP_OK;
}

}  // namespace

esp_err_t startSetupPortal(std::uint8_t channel, std::uint32_t gatewayId) {
    wifi_config_t ap{};
    const int ssidLen =
        std::snprintf(reinterpret_cast<char*>(ap.ap.ssid), sizeof(ap.ap.ssid), "SensorNet-%04lx",
                      static_cast<unsigned long>(gatewayId & 0xFFFFU));
    ap.ap.ssid_len = static_cast<std::uint8_t>(ssidLen);
    std::strncpy(reinterpret_cast<char*>(ap.ap.password), CONFIG_SN_SETUP_AP_PASSWORD,
                 sizeof(ap.ap.password) - 1);
    ap.ap.channel = channel;
    ap.ap.max_connection = 2;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        return err;
    }

    httpd_handle_t server = nullptr;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144;  // the save handler keeps the form body on its stack
    if ((err = httpd_start(&server, &config)) != ESP_OK) {
        return err;
    }
    const httpd_uri_t index{
        .uri = "/", .method = HTTP_GET, .handler = handleIndex, .user_ctx = nullptr};
    const httpd_uri_t save{
        .uri = "/save", .method = HTTP_POST, .handler = handleSave, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &index);
    httpd_register_uri_handler(server, &save);

    ESP_LOGW(kTag, "SETUP MODE: join Wi-Fi \"%s\" (password \"%s\") and open http://192.168.4.1",
             reinterpret_cast<const char*>(ap.ap.ssid), CONFIG_SN_SETUP_AP_PASSWORD);
    return ESP_OK;
}

}  // namespace gw
