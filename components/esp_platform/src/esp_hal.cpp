#include "sn/esp/esp_hal.hpp"

#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sn::esp {

std::uint64_t EspClock::nowUs() const {
    return static_cast<std::uint64_t>(esp_timer_get_time());
}

void EspClock::delayUs(std::uint32_t us) {
    esp_rom_delay_us(us);
}

void EspClock::delayMs(std::uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

EspOutputPin::EspOutputPin(gpio_num_t pin) : pin_(pin) {
    const gpio_config_t cfg{
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(pin_, 0);
}

void EspOutputPin::write(bool level) {
    gpio_set_level(pin_, level ? 1 : 0);
}

EspInputPin::EspInputPin(gpio_num_t pin, gpio_pull_mode_t pull) : pin_(pin) {
    const gpio_config_t cfg{
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull == GPIO_PULLUP_ONLY ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = pull == GPIO_PULLDOWN_ONLY ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

bool EspInputPin::read() const {
    return gpio_get_level(pin_) != 0;
}

}  // namespace sn::esp
