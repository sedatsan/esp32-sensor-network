#pragma once

#include <cstdint>
#include <optional>

#include "sn/hal/clock.hpp"
#include "sn/hal/gpio.hpp"

namespace sn::drivers {

// Driver for the HC-SR501 passive infrared motion sensor.
//
// The module output is high while motion is detected (in retrigger mode it
// stays high as long as motion continues). After power-up it needs a warm-up
// period during which the output is unreliable.
class Pir {
public:
    enum class Event : std::uint8_t { MotionStarted, MotionEnded };

    struct Config {
        std::uint32_t warmupUs = 60'000'000;  // datasheet: up to 60 s
    };

    Pir(hal::IInputPin& out, hal::IClock& clock, const Config& config);
    Pir(hal::IInputPin& out, hal::IClock& clock) : Pir(out, clock, Config{}) {}

    // Marks the sensor as already warmed up, e.g. when waking from deep sleep
    // while the PIR stayed powered.
    void skipWarmup() { warmedUp_ = true; }

    [[nodiscard]] bool isReady() const;
    [[nodiscard]] bool motionDetected() const;

    // Poll for edges; returns an event when the state changed since the last
    // call. Ignores the output during warm-up.
    std::optional<Event> poll();

private:
    hal::IInputPin& out_;
    hal::IClock& clock_;
    Config config_;
    std::uint64_t startUs_;
    bool warmedUp_ = false;
    bool lastLevel_ = false;
};

}  // namespace sn::drivers
