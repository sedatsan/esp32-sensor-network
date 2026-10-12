#pragma once

#include "driver/gpio.h"
#include "sn/app/node_controller.hpp"
#include "sn/hal/clock.hpp"
#include "sn/hal/gpio.hpp"

namespace sn::esp {

class EspClock final : public hal::IClock, public app::IClockDelay {
public:
    std::uint64_t nowUs() const override;
    void delayUs(std::uint32_t us) override;
    void delayMs(std::uint32_t ms) override;
};

class EspOutputPin final : public hal::IOutputPin {
public:
    explicit EspOutputPin(gpio_num_t pin);
    void write(bool level) override;

private:
    gpio_num_t pin_;
};

class EspInputPin final : public hal::IInputPin {
public:
    EspInputPin(gpio_num_t pin, gpio_pull_mode_t pull);
    bool read() const override;

private:
    gpio_num_t pin_;
};

}  // namespace sn::esp
